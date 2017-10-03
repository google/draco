
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


### mem_get_le16

~~~~~
uint32_t mem_get_le16(mem) {
  val = mem[1] << 8;
  val |= mem[0];
  return val;
}
~~~~~
{:.draco-syntax }


### mem_get_le24

~~~~~
uint32_t mem_get_le24(mem) {
  val = mem[2] << 16;
  val |= mem[1] << 8;
  val |= mem[0];
  return val;
}
~~~~~
{:.draco-syntax }


### mem_get_le32

~~~~~
uint32_t mem_get_le32(mem) {
  val = mem[3] << 24;
  val |= mem[2] << 16;
  val |= mem[1] << 8;
  val |= mem[0];
  return val;
}
~~~~~
{:.draco-syntax }
