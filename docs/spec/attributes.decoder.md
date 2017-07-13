
## Attributes Decoder

### DecodeAttributesDecoderData()

~~~~~
DecodeAttributesDecoderData(buffer) {
  num_attributes                                                        I32
  point_attribute_ids_.resize(num_attributes);
  for (i = 0; i < num_attributes; ++i) {
    att_type                                                            UI8
    data_type                                                           UI8
    components_count                                                    UI8
    normalized                                                          UI8
    custom_id                                                           UI16
    Initialize GeometryAttribute ga
    att_id = pc->AddAttribute(new PointAttribute(ga));
    point_attribute_ids_[i] = att_id;
}
~~~~~
{:.draco-syntax }
