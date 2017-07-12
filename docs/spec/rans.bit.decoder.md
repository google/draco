
## Rans Bit Decoder

### RansBitDecoder_StartDecoding()

~~~~~
RansBitDecoder_StartDecoding(DecoderBuffer *source_buffer) {
  prob_zero_                                                            UI8
  size                                                                  UI32
  buffer_                                                               size * UI8
  ans_read_init(&ans_decoder_, buffer_, size)
}
~~~~~
{:.draco-syntax }


### DecodeNextBit()

~~~~~
DecodeNextBit() {
  uint8_t bit = rabs_desc_read(&ans_decoder_, prob_zero_);
  return bit > 0;
}
~~~~~
{:.draco-syntax }
