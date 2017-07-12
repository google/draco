
## Sequential Quantization Attribute Decoder

~~~~~
Initialize(...) {
  SequentialIntegerAttributeDecoder_Initialize()
}
~~~~~
{:.draco-syntax }


### DecodeIntegerValues()

~~~~~
DecodeIntegerValues(point_ids) {
  // DecodeQuantizedDataInfo()
  num_components = attribute()->components_count();
  for (i = 0; i < num_components; ++i) {
    min_value_[i]                                                       F32
  }
  max_value_dif_                                                        F32
  quantization_bits_                                                    UI8
  SequentialIntegerAttributeDecoder::DecodeIntegerValues()
}
~~~~~
{:.draco-syntax }


### DequantizeValues()

~~~~~
DequantizeValues(num_values) {
  max_quantized_value = (1 << (quantization_bits_)) - 1;
  num_components = attribute()->components_count();
  entry_size = sizeof(float) * num_components;
  quant_val_id = 0;
  out_byte_pos = 0;
  for (i = 0; i < num_values; ++i) {
    for (c = 0; c < num_components; ++c) {
      value = dequantizer.DequantizeFloat(values()->at(quant_val_id++));
      value = value + min_value_[c];
      att_val[c] = value;
    }
    attribute()->buffer()->Write(out_byte_pos, att_val.get(), entry_size);
    out_byte_pos += entry_size;
  }
}
~~~~~
{:.draco-syntax }
