
## Prediction Scheme Transform

### ComputeOriginalValue()

~~~~~
ComputeOriginalValue(const DataTypeT *predicted_vals,
                     const CorrTypeT *corr_vals,
                     DataTypeT *out_original_vals, int val_id) {
  for (i = 0; i < num_components_; ++i) {
    out_original_vals[i] = predicted_vals[i] + corr_vals[val_id + i];
  }
}
~~~~~
{:.draco-syntax }
