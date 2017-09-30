
## Core Functions

### DecodeVarint

~~~~~
void DecodeVarint(out_val) {
  in                                                                                  UI8
  if (in & (1 << 7)) {
    DecodeVarint(out_val);
    out_val = (out_val << 7) | (in & ((1 << 7) - 1));
  } else {
    out_val = in;
  }
}
~~~~~
{:.draco-syntax }
