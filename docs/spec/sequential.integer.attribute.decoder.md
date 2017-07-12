
## Sequential Integer Attribute Decoder

~~~~~
Initialize(...) {
  SequentialAttributeDecoder_Initialize()
}
~~~~~
{:.draco-syntax }


### DecodeValues()

~~~~~
DecodeValues(point_ids) {
  prediction_scheme_method                                              I8
  if (prediction_scheme_method != PREDICTION_NONE) {
    prediction_transform_type                                           I8
    prediction_scheme_ = CreateIntPredictionScheme(...)
  }
  if (prediction_scheme_) {
  }
  DecodeIntegerValues(point_ids)
  //SequentialQuantizationAttributeDecoder_DecodeIntegerValues()
  //StoreValues()
  DequantizeValues(num_values)
}
~~~~~
{:.draco-syntax }


### DecodeIntegerValues()

~~~~~
DecodeIntegerValues(point_ids) {
  compressed                                                            UI8
  if (compressed) {
    DecodeSymbols(..., values_.data())
  } else {
  // TODO
  }
  if (!prediction_scheme_->AreCorrectionsPositive()) {
    ConvertSymbolsToSignedInts(...)
  }
  if (prediction_scheme_) {
    prediction_scheme_->DecodePredictionData(buffer)
    // DecodeTransformData(buffer)
    if (!values_.empty()) {
      prediction_scheme_->Decode(values_.data(), &values_[0],
                                 values_.size(), num_components, point_ids.data())
      // MeshPredictionSchemeParallelogram_Decode()
}
~~~~~
{:.draco-syntax }

