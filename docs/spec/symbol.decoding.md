
## Symbol Decoding

### DecodeSymbols()

~~~~~
DecodeSymbols(num_symbols, out_buffer, out_values) {
  scheme                                                                UI8
  If (scheme == 0) {
    DecodeTaggedSymbols<>(num_symbols, src_buffer, out_values)
  } else if (scheme == 1) {
    DecodeRawSymbols<>(num_symbols, src_buffer, out_values)
  }
}
~~~~~
{:.draco-syntax }


### DecodeTaggedSymbols()

~~~~~
DecodeTaggedSymbols() {
  FIXME
}
~~~~~
{:.draco-syntax }


### DecodeRawSymbols()

~~~~~
DecodeRawSymbols() {
  max_bit_length                                                        UI8
  DecodeRawSymbolsInternal(max_bit_length, out_values)
  return symbols
}
~~~~~
{:.draco-syntax }


### DecodeRawSymbolsInternal()

~~~~~
DecodeRawSymbolsInternal(max_bit_length, out_values) {
  decoder = CreateRansSymbolDecoder(max_bit_length)
  decoder.StartDecoding()
  // RansSymbolDecoder_StartDecoding
  for (i = 0; i < num_values; ++i) {
    out_values[i] = decoder.DecodeSymbol()
    // RansSymbolDecoder_DecodeSymbol
  }
}
~~~~~
{:.draco-syntax }


### CreateRansSymbolDecoder()

~~~~~
CreateRansSymbolDecoder(max_bit_length) {
  rans_precision_bits  = (3 * max_bit_length) / 2;
  rans_precision_bits = min(rans_precision_bits, 20)
  rans_precision_bits = max(rans_precision_bits, 12)
  rans_precision = 1 << rans_precision_bits_;
  l_rans_base = rans_precision * 4;
  num_symbols_                                                          UI32
  for (i = 0; i < num_symbols_; ++i) {
    prob_data                                                           UI8
    if ((prob_data & 3) == 3) {
      offset = prob_data >> 2
      for (j = 0; j < offset + 1; ++j) {
        probability_table_[i + j] = 0;
      }
      i += offset;
    } else {
      prob = prob_data >> 2
      for (j = 0; j < token; ++j) {
        eb                                                              UI8
        prob = prob | (eb << (8 * (j + 1) - 2)
      }
      probability_table_[i] = prob;
    }
  }
  rans_build_look_up_table()
}
~~~~~
{:.draco-syntax }


### RansSymbolDecoder_StartDecoding()

~~~~~
RansSymbolDecoder_StartDecoding() {
  bytes_encoded                                                         UI64
  buffer                                                                bytes_encoded * UI8
  rans_read_init(buffer, bytes_encoded)
}
~~~~~
{:.draco-syntax }


### RansSymbolDecoder_DecodeSymbol()

~~~~~
RansSymbolDecoder_DecodeSymbol() {
  ans_.rans_read()
}
~~~~~
{:.draco-syntax }
