
## Rans Decoding

### ans_read_init()

~~~~~
ans_read_init(struct AnsDecoder *const ans, const uint8_t *const buf,
                       int offset) {
  x = buf[offset - 1] >> 6
  If (x == 0) {
    ans->buf_offset = offset - 1;
    ans->state = buf[offset - 1] & 0x3F;
  } else if (x == 1) {
    ans->buf_offset = offset - 2;
    ans->state = mem_get_le16(buf + offset - 2) & 0x3FFF;
  } else if (x == 2) {
    ans->buf_offset = offset - 3;
    ans->state = mem_get_le24(buf + offset - 3) & 0x3FFFFF;
  } else if (x == 3) {
   // x == 3 implies this byte is a superframe marker
    return 1;
  }
  ans->state += l_base;
}
~~~~~
{:.draco-syntax }


### int rabs_desc_read()

~~~~~
int rabs_desc_read(struct AnsDecoder *ans, AnsP8 p0) {
  AnsP8 p = ans_p8_precision - p0;
  if (ans->state < l_base) {
    ans->state = ans->state * io_base + ans->buf[--ans->buf_offset];
  }
  x = ans->state;
  quot = x / ans_p8_precision;
  rem = x % ans_p8_precision;
  xn = quot * p;
  val = rem < p;
  if (val) {
    ans->state = xn + rem;
  } else {
    ans->state = x - xn - p;
  }
  return val;
}
~~~~~
{:.draco-syntax }


### rans_read_init()

~~~~~
rans_read_init(UI8 *buf, int offset) {
  ans_.buf = buf;
  x = buf[offset - 1] >> 6
  If (x == 0) {
    ans_.buf_offset = offset - 1;
    ans_.state = buf[offset - 1] & 0x3F;
  } else if (x == 1) {
    ans_.buf_offset = offset - 2;
    ans_.state = mem_get_le16(buf + offset - 2) & 0x3FFF;
  } else if (x == 2) {
    ans_.buf_offset = offset - 3;
    ans_.state = mem_get_le24(buf + offset - 3) & 0x3FFFFF;
  } else if (x == 3) {
    ans_.buf_offset = offset - 4;
    ans_.state = mem_get_le32(buf + offset - 4) & 0x3FFFFFFF;
  }
  ans_.state += l_rans_base;
}
~~~~~
{:.draco-syntax }


### rans_build_look_up_table()

~~~~~
rans_build_look_up_table() {
  cum_prob = 0
  act_prob = 0
  for (i = 0; i < num_symbols; ++i) {
    probability_table_[i].prob = token_probs[i];
    probability_table_[i].cum_prob = cum_prob;
    cum_prob += token_probs[i];
    for (j = act_prob; j < cum_prob; ++j) {
      Lut_table_[j] = i
    }
    act_prob = cum_prob
}
~~~~~
{:.draco-syntax }


### rans_read()

~~~~~
rans_read() {
  while (ans_.state < l_rans_base) {
    ans_.state = ans_.state * io_base + ans_.buf[--ans_.buf_offset];
  }
  quo = ans_.state / rans_precision;
  rem = ans_.state % rans_precision;
  sym = fetch_sym()
  ans_.state = quo * sym.prob + rem - sym.cum_prob;
  return sym.val;
}
~~~~~
{:.draco-syntax }


### fetch_sym()

~~~~~
fetch_sym() {
  symbol = lut_table[rem]
  out->val = symbol
  out->prob = probability_table_[symbol].prob;
  out->cum_prob = probability_table_[symbol].cum_prob;
}
~~~~~
{:.draco-syntax }
