
## Core Functions

### DecodeVarint<IT>

~~~~~
DecodeVarint<IT>() {
  If (std::is_unsigned<IT>::value) {
    in                                                                  UI8
    If (in & (1 << 7)) {
      out = DecodeVarint<IT>()
      out = (out << 7) | (in & ((1 << 7) - 1))
    } else {
      typename std::make_unsigned<IT>::type UIT;
      out = DecodeVarint<UIT>()
      out = ConvertSymbolToSignedInt(out)
    }
    return out;
}
~~~~~
{:.draco-syntax }


### ConvertSymbolToSignedInt()

~~~~~
ConvertSymbolToSignedInt() {
  abs_val = val >> 1
  If (val & 1 == 0) {
    return abs_val
  } else {
    signed_val = -abs_val - 1
  }
  return signed_val
}
~~~~~
{:.draco-syntax }


Sequential Decoder

FIXME: ^^^ Heading level?

### decode_connectivity()

~~~~~
decode_connectivity() {
  num_faces                                                             I32
  num_points                                                            I32
  connectivity _method                                                  UI8
  If (connectivity _method == 0) {
    // TODO
  } else {
    loop num_faces {
      If (num_points < 256) {
        face[]                                                          UI8
      } else if (num_points < (1 << 16)) {
        face[]                                                          UI16
      } else {
        face[]                                                          UI32
      }
    }
  }
}
~~~~~
{:.draco-syntax }
