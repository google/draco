# Kotoba v1 — Draco magic and header

This directory is a first-class language tree on the `kotoba-lang/draco`
fork, next to `src/` (C++) and `javascript/`. It is **not** part of
google/draco upstream.

## Honest scope

Kotoba binding **v1** parses only:

- the Draco bitstream magic `DRACO` (5 bytes)
- enough of `DracoHeader` to identify a tiny vendored mesh fixture:
  bitstream **version** (`version_major`, `version_minor`) and
  **encoded type** (`encoder_type`, plus `encoder_method` and `flags`)

from the 11-byte fixture `fixtures/tiny-mesh.drc`. Field order matches
`DracoHeader` in `src/draco/compression/config/compression_shared.h` and
`PointCloudDecoder::DecodeHeader`.

The fixture is a hand-built header prefix (magic + version 2.2 +
`TRIANGULAR_MESH` + `MESH_EDGEBREAKER_ENCODING` + flags 0). It is enough
to identify that bitstream. It is **not** a complete mesh file.

This is **not** a replacement for the C++ or JavaScript libraries. It
does not decode connectivity, attributes, metadata, or point clouds. It
does not encode. It is **not robotics-ready**.

## Language constraints

- Kotoba CLI **0.7.2**
- `kotoba compile --target wasm` → `wasm32-kotoba-v1`
- value profile **i64-v1** (no IEEE floats)
- no FFI / no host imports

`draco.kotoba` embeds the fixture as integer bytes and uses only `+`,
`*`, `if`, `=`, and `and`. `main` returns a packed i64 of the parsed
fields when magic and encoded type identify a triangular mesh.

## Checks

`checks.sh` compiles with Kotoba 0.7.2, runs the module, and compares
the packed result to fields read from the fixture bytes. It does not
invent pass/fail.

```sh
# requires Linux amd64, or KOTOBA pointing at a 0.7.2 CLI
./checks.sh
```

## Operator

awai.network / Ryo Awai
