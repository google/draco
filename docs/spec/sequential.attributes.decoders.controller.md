
## Sequential Attributes Decoders Controller

### DecodeAttributesDecoderData()

~~~~~
DecodeAttributesDecoderData(buffer) {
  AttributesDecoder_DecodeAttributesDecoderData(buffer)
  sequential_decoders_.resize(num_attributes());
  for (i = 0; i < num_attributes(); ++i) {
    decoder_type                                                        UI8
    sequential_decoders_[i] = CreateSequentialDecoder(decoder_type);
    sequential_decoders_[i]->Initialize(decoder(), GetAttributeId(i))
}
~~~~~
{:.draco-syntax }


### DecodeAttributes()

~~~~~
DecodeAttributes(buffer) {
  sequencer_->GenerateSequence(&point_ids_)
  for (i = 0; i < num_attributes(); ++i) {
    pa = decoder()->point_cloud()->attribute(GetAttributeId(i));
    sequencer_->UpdatePointToAttributeIndexMapping(pa)
  }
  for (i = 0; i < num_attributes(); ++i) {
    sequential_decoders_[i]->Decode(point_ids_, buffer)
    //SequentialAttributeDecoder_Decode()
  }
}
~~~~~
{:.draco-syntax }


### CreateSequentialDecoder()

~~~~~
CreateSequentialDecoder(type) {
  switch (type) {
    case SEQUENTIAL_ATTRIBUTE_ENCODER_GENERIC:
      return new SequentialAttributeDecoder()
    case SEQUENTIAL_ATTRIBUTE_ENCODER_INTEGER:
      return new SequentialIntegerAttributeDecoder()
    case SEQUENTIAL_ATTRIBUTE_ENCODER_QUANTIZATION:
      return new SequentialQuantizationAttributeDecoder()
    case SEQUENTIAL_ATTRIBUTE_ENCODER_NORMALS:
      return new SequentialNormalAttributeDecoder()
  }
}
~~~~~
{:.draco-syntax }

