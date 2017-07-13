
## Prediction Scheme Wrap Transform

### DecodeTransformData()

~~~~~
DecodeTransformData(buffer) {
  min_value_                                                            DT
  max_value_                                                            DT
}
~~~~~
{:.draco-syntax }


### ComputeOriginalValue()

~~~~~
ComputeOriginalValue(const DataTypeT *predicted_vals,
                     const CorrTypeT *corr_vals,
                     DataTypeT *out_original_vals, int val_id) {
  clamped_vals = ClampPredictedValue(predicted_vals);
  ComputeOriginalValue(clamped_vals, corr_vals, out_original_vals, val_id)
  // PredictionSchemeTransform_ComputeOriginalValue()
  for (i = 0; i < this->num_components(); ++i) {
    if (out_original_vals[i] > max_value_) {
      out_original_vals[i] -= max_dif_;
    } else if (out_original_vals[i] < min_value_) {
      out_original_vals[i] += max_dif_;
    }
}
~~~~~
{:.draco-syntax }


### ClampPredictedValue()

~~~~~
ClampPredictedValue(const DataTypeT *predicted_val) {
  for (i = 0; i < this->num_components(); ++i) {
    clamped_value_[i] = min(predicted_val[i], max_value_)
    clamped_value_[i] = max(predicted_val[i], min_value_)
  }
  return &clamped_value_[0];
}
~~~~~
{:.draco-syntax }
