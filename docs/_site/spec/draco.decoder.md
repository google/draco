## Draco Decoder

### Decode()

<div class="syntax">
Decode() {                                                            <b>Type</b>
  DecodeHeader()
  DecodeConnectivityData()
  DecodeAttributeData()}

</div>


### DecodeHeader()

<div class="syntax">
DecodeHeader() {                                                      <b>Type</b>
  <b>draco_string</b>                                                        UI8[5]
  <b>major_version</b>                                                       UI8
  <b>minor_version</b>                                                       UI8
  <b>encoder_type</b>                                                        UI8
  <b>encoder_method</b>                                                      UI8
  flags
}

</div>


### DecodeAttributeData()

<div class="syntax">
DecodeAttributeData() {                                               <b>Type</b>
  <b>num_attributes_decoders</b>                                             UI8
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
</div>
