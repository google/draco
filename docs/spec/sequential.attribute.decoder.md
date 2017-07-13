
## Sequential Attribute Decoder

~~~~~
Initialize(...) {
  // Init some members
}
~~~~~
{:.draco-syntax }


### DecodeValues()

~~~~~
DecodeValues(const std::vector<PointIndex> &point_ids) {
  num_values = point_ids.size();
  entry_size = attribute_->byte_stride();
  std::unique_ptr<uint8_t[]> value_data_ptr(new uint8_t[entry_size]);
  out_byte_pos = 0;
  for (i = 0; i < num_values; ++i) {
   value_data                                                           UI8 * entry_size
    attribute_->buffer()->Write(out_byte_pos, value_data, entry_size);
    out_byte_pos += entry_size;
  }
}
~~~~~
{:.draco-syntax }

