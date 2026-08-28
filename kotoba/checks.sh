#!/usr/bin/env bash
# Compile draco.kotoba with Kotoba 0.7.2 (wasm32, i64-v1) and assert
# magic + header fields against the vendored mesh fixture.
# Fail closed. Do not print success unless every comparison ran.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
FIXTURE="$ROOT/fixtures/tiny-mesh.drc"
SRC="$ROOT/draco.kotoba"
KOTOBA_VERSION="0.7.2"
KOTOBA_TARBALL="kotoba-linux-amd64.tar.gz"
# sha256 of https://github.com/kotoba-lang/kotoba/releases/download/v0.7.2/kotoba-linux-amd64.tar.gz
KOTOBA_SHA256="95e225461e1b8a21849b251e8c8b654693d2c8a516b258532771651e978e1977"

WORKDIR="$(mktemp -d "${TMPDIR:-/tmp}/kotoba-draco-v1.XXXXXX")"
trap 'rm -rf "$WORKDIR"' EXIT

fail() {
  printf 'kotoba/checks.sh: %s\n' "$*" >&2
  exit 1
}

command -v python3 >/dev/null 2>&1 || fail "python3 is required"

if [[ -n "${KOTOBA:-}" ]]; then
  KOTOBA_BIN="${KOTOBA}"
  [[ -x "${KOTOBA_BIN}" ]] || fail "KOTOBA=${KOTOBA_BIN} is not executable"
elif command -v kotoba >/dev/null 2>&1; then
  KOTOBA_BIN="$(command -v kotoba)"
  CURRENT_LINK="${KOTOBA_HOME:-$HOME/.local/share/kotoba}/current"
  if [ -L "$CURRENT_LINK" ]; then
    INSTALLED="$(readlink "$CURRENT_LINK")"
    printf 'kotoba install current: %s\n' "$INSTALLED"
    if [ "$INSTALLED" != "v0.7.2" ]; then
      fail "refusing kotoba $INSTALLED (need v0.7.2)"
    fi
  fi
else
  uname_s="$(uname -s)"
  uname_m="$(uname -m)"
  if [[ "${uname_s}" != "Linux" || "${uname_m}" != "x86_64" ]]; then
    fail "no KOTOBA set; automatic install is linux-amd64 only (this host is ${uname_s}/${uname_m})"
  fi
  cache="${ROOT}/.kotoba-cli/${KOTOBA_VERSION}"
  mkdir -p "${cache}"
  archive="${cache}/${KOTOBA_TARBALL}"
  if [[ ! -x "${cache}/kotoba" ]]; then
    url="https://github.com/kotoba-lang/kotoba/releases/download/v${KOTOBA_VERSION}/${KOTOBA_TARBALL}"
    printf 'downloading Kotoba %s from %s\n' "${KOTOBA_VERSION}" "${url}"
    curl -fsSL -o "${archive}" "${url}"
    got="$(sha256sum "${archive}" | awk '{print $1}')"
    if [[ "${got}" != "${KOTOBA_SHA256}" ]]; then
      fail "checksum mismatch for ${KOTOBA_TARBALL}: got ${got} expected ${KOTOBA_SHA256}"
    fi
    tar -xzf "${archive}" -C "${cache}" kotoba
  fi
  KOTOBA_BIN="${cache}/kotoba"
  [[ -x "${KOTOBA_BIN}" ]] || fail "extracted kotoba binary missing"
fi

printf 'kotoba binary: %s\n' "$KOTOBA_BIN"

[ -f "$FIXTURE" ] || fail "missing fixture $FIXTURE"
[ -f "$SRC" ] || fail "missing module $SRC"

# Independent field read from the fixture. These numbers come from the
# file bytes, not from the .kotoba source.
FIELDS="$WORKDIR/fields.env"
set +e
python3 - "$FIXTURE" "$SRC" >"$FIELDS" 2>"$WORKDIR/fields.err" <<'PY'
import sys
from pathlib import Path

fixture = Path(sys.argv[1])
src = Path(sys.argv[2]).read_text()
raw = fixture.read_bytes()
if len(raw) != 11:
    print("fixture must be exactly the 11-byte DracoHeader", file=sys.stderr)
    sys.exit(1)
if raw[:5] != b"DRACO":
    print("fixture does not start with DRACO magic", file=sys.stderr)
    sys.exit(1)
for i, b in enumerate(raw):
    needle = f"(= i {i}) {b}"
    if needle not in src:
        print(f"draco.kotoba is missing fixture byte {i} = {b}", file=sys.stderr)
        sys.exit(1)
major = raw[5]
minor = raw[6]
encoder_type = raw[7]
encoder_method = raw[8]
flags = int.from_bytes(raw[9:11], "little")
if encoder_type != 1:
    print(f"this v1 check identifies a triangular mesh (got encoder_type {encoder_type})", file=sys.stderr)
    sys.exit(1)
packed = 1 + 10 * major + 100 * minor + 1000 * encoder_type + 10000 * encoder_method + 1000000 * flags
print(f"FIXTURE_LEN={len(raw)}")
print(f"FIELD_MAJOR={major}")
print(f"FIELD_MINOR={minor}")
print(f"FIELD_TYPE={encoder_type}")
print(f"FIELD_METHOD={encoder_method}")
print(f"FIELD_FLAGS={flags}")
print(f"PACKED_EXPECT={packed}")
PY
fields_rc=$?
set -e
if [ "$fields_rc" -ne 0 ]; then
  cat "$WORKDIR/fields.err" >&2 || true
  fail "fixture field read failed"
fi
# shellcheck disable=SC1090
. "$FIELDS"

printf 'fixture: %s (%s bytes)\n' "$FIXTURE" "$FIXTURE_LEN"
printf 'fixture fields: major=%s minor=%s encoder_type=%s encoder_method=%s flags=%s\n' \
  "$FIELD_MAJOR" "$FIELD_MINOR" "$FIELD_TYPE" "$FIELD_METHOD" "$FIELD_FLAGS"
printf 'packed expect (from fixture bytes): %s\n' "$PACKED_EXPECT"

COMPILE_JSON="$WORKDIR/compile.json"
WASM="$WORKDIR/draco.wasm"
set +e
"$KOTOBA_BIN" compile "$SRC" --target wasm -o "$WASM" --json >"$COMPILE_JSON" 2>"$WORKDIR/compile.err"
compile_rc=$?
set -e
if [ "$compile_rc" -ne 0 ]; then
  cat "$COMPILE_JSON" "$WORKDIR/compile.err" >&2 || true
  fail "kotoba compile failed (exit $compile_rc)"
fi

python3 - "$COMPILE_JSON" "$WASM" <<'PY'
import json
import sys
from pathlib import Path

report_text = Path(sys.argv[1]).read_text()
wasm = Path(sys.argv[2])
try:
    report = json.loads(report_text)
except json.JSONDecodeError:
    sys.exit(f"compile --json was not JSON:\n{report_text}")
if report.get("kotoba.cli/ok?") is not True:
    sys.exit(f"compile JSON ok? is {report.get('kotoba.cli/ok?')!r}")
if report.get("kotoba.cli/code") != "emitted":
    sys.exit(f"compile JSON code is {report.get('kotoba.cli/code')!r}")
data = report.get("kotoba.cli/data") or {}
profile = data.get("value-profile")
compat = data.get("compatibility") or {}
target = compat.get("target")
abi = compat.get("value-abi")
features = data.get("wasm-features") or []
if profile != "i64-v1":
    sys.exit(f"value-profile {profile!r} is not i64-v1")
if target != "wasm32-kotoba-v1":
    sys.exit(f"target {target!r} is not wasm32-kotoba-v1")
if abi not in (None, "direct-v1"):
    sys.exit(f"value-abi {abi!r} is not direct-v1")
blocked = [f for f in features if f in ("simd", "floats", "float", "nontrapping-fptoint")]
if blocked:
    sys.exit(f"unexpected floating/SIMD wasm features: {blocked}")
if not wasm.is_file() or wasm.stat().st_size == 0:
    sys.exit("compile did not write a wasm artifact")
blob = wasm.read_bytes()
if blob[:4] != b"\x00asm":
    sys.exit(f"artifact magic {blob[:4]!r} is not wasm")

def read_u32(buf, i):
    n = 0
    shift = 0
    while True:
        b = buf[i]
        i += 1
        n |= (b & 0x7F) << shift
        if b < 0x80:
            return n, i
        shift += 7

i = 8
imports = False
while i < len(blob):
    sid = blob[i]
    i += 1
    size, i = read_u32(blob, i)
    if sid == 2:
        imports = True
    i += size
if imports:
    sys.exit("wasm has an import section; host-independent i64-v1 must not")
print(f"compile: value-profile={profile} target={target} value-abi={abi} wasm-features={features} bytes={len(blob)} host-independent")
PY

# Prefer kotoba run (observed integer :kotoba.runtime/value). Node wasm
# instantiate is a fallback only if the CLI run path is unavailable.
set +e
"$KOTOBA_BIN" run "$SRC" >"$WORKDIR/run.out" 2>"$WORKDIR/run.err"
run_rc=$?
set -e

GOT=""
if [ "$run_rc" -eq 0 ]; then
  GOT="$(python3 - "$WORKDIR/run.out" "$WORKDIR/run.err" <<'PY'
import re
import sys
from pathlib import Path

text = Path(sys.argv[1]).read_text() + "\n" + Path(sys.argv[2]).read_text()
if ":kotoba.runtime/ok? true" not in text:
    sys.exit("kotoba run produced no :kotoba.runtime/ok? true")
match = re.search(r":kotoba.runtime/value (-?\d+)", text)
if match is None:
    sys.exit("kotoba run produced no integer :kotoba.runtime/value")
print(match.group(1))
PY
)" || GOT=""
fi

if [ -z "$GOT" ]; then
  command -v node >/dev/null 2>&1 || fail "kotoba run did not yield a value and node is not available to instantiate wasm"
  printf 'kotoba run did not yield :kotoba.runtime/value; instantiating wasm with node\n'
  GOT="$(node --input-type=module - "$WASM" <<'JS'
import fs from "node:fs";
const wasm = fs.readFileSync(process.argv[2]);
const { instance } = await WebAssembly.instantiate(wasm);
if (!instance.exports.main) {
  throw new Error("wasm module has no exported main");
}
const value = instance.exports.main();
const n = typeof value === "bigint" ? value : BigInt(value);
process.stdout.write(n.toString());
JS
)"
fi

printf 'module returned: %s\n' "$GOT"
if [ "$GOT" != "$PACKED_EXPECT" ]; then
  fail "packed result $GOT != fixture-derived $PACKED_EXPECT"
fi

printf 'kotoba/checks.sh: compile i64-v1 wasm32 and fixture fields matched\n'
