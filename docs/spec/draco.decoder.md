## Draco Decoder

### Decode()

~~~~~
Decode() {
  DecodeHeader()
  DecodeConnectivityData()
  DecodeAttributeData()}
~~~~~
{:.draco-syntax}


### DecodeHeader()

~~~~~
DecodeHeader() {
  draco_string                                                          UI8[5]
  major_version                                                         UI8
  minor_version                                                         UI8
  encoder_type                                                          UI8
  encoder_method                                                        UI8
  flags
}
~~~~~
{:.draco-syntax}


### DecodeAttributeData()

~~~~~
DecodeAttributeData() {
  num_attributes_decoders                                               UI8
  for (i = 0; i < num_attributes_decoders; ++i) {
    CreateAttributesDecoder(i);
  }
  for (auto &att_dec : attributes_decoders_) {
    att_dec->Initialize(this, point_cloud_)
  }
  for (i = 0; i < num_attributes_decoders; ++i) {
    attributes_decoders_[i]->DecodeAttributesDecoderData(buffer_)
  }
  DecodeAllAttributes()
  OnAttributesDecoded()
~~~~~
{:.draco-syntax}
