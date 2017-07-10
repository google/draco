
## Edgebreaker Traversal Decoder

### EdgebreakerTraversal_Start()

<div class="syntax">
EdgebreakerTraversal_Start() {                                        <b>Type</b>
  <b>size</b>                                                                UI64
  <b>symbol_buffer_</b>                                                      size * UI8
  <b>size</b>                                                                UI64
  <b>start_face_buffer_</b>                                                  size * UI8
  if (num_attribute_data_ > 0) {
    attribute_connectivity_decoders_ = std::unique_ptr<BinaryDecoder[]>(
          new BinaryDecoder[num_attribute_data_]);
    for (i = 0; i < num_attribute_data_; ++i) {
      attribute_connectivity_decoders_[i].StartDecoding()
      // RansBitDecoder_StartDecoding
  }
}

</div>


### Traversal_DecodeSymbol()

~~~~~
Traversal_DecodeSymbol() {
  symbol_buffer_.DecodeLeastSignificantBits32(1, &symbol);                   bits1
  if (symbol != TOPOLOGY_C) {
    symbol_buffer_.DecodeLeastSignificantBits32(2, &symbol_suffix);          bits2
    symbol |= (symbol_suffix << 1);
  }
  return symbol
}
~~~~~


### DecodeAttributeSeam()

~~~~~
DecodeAttributeSeam(int attribute) {
  return attribute_connectivity_decoders_[attribute].DecodeNextBit();
}
~~~~~
