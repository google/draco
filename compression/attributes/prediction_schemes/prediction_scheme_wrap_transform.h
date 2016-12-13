// Copyright 2016 The Draco Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
#ifndef DRACO_COMPRESSION_ATTRIBUTES_PREDICTION_SCHEMES_PREDICTION_SCHEME_WRAP_TRANSFORM_H_
#define DRACO_COMPRESSION_ATTRIBUTES_PREDICTION_SCHEMES_PREDICTION_SCHEME_WRAP_TRANSFORM_H_

#include "compression/attributes/prediction_schemes/prediction_scheme.h"

namespace draco {

// PredictionSchemeWrapTransform uses the min and max bounds of the original
// data to wrap stored correction values around these bounds centered at 0,
// i.e., when the range of the original values O is between <MIN, MAX> and
// N = MAX-MIN, we can then store any correction X = O - P, as:
//        X + N,   if X < -N / 2
//        X - N,   if X > N / 2
//        X        otherwise
// To unwrap this value, the decoder then simply checks whether the final
// corrected value F = P + X is out of the bounds of the input values.
// All out of bounds values are unwrapped using
//        F + N,   if F < MIN
//        F - N,   if F > MAX
// This wrapping can reduce the number of unique values, which translates to a
// better entropy of the stored values and better compression rates.
template <typename DataTypeT, typename CorrTypeT = DataTypeT>
class PredictionSchemeWrapTransform
    : public PredictionSchemeTransform<DataTypeT, CorrTypeT> {
 public:
  typedef CorrTypeT CorrType;
  PredictionSchemeWrapTransform()
      : min_value_(0),
        max_value_(0),
        max_dif_(0),
        max_correction_(0),
        min_correction_(0) {}

  PredictionSchemeTransformType GetType() const {
    return PREDICTION_TRANSFORM_WRAP;
  }

  void InitializeEncoding(const DataTypeT *orig_data, int size,
                          int num_components) {
    PredictionSchemeTransform<DataTypeT, CorrTypeT>::InitializeEncoding(
        orig_data, size, num_components);
    // Go over the original values and compute the bounds.
    if (size == 0)
      return;
    min_value_ = max_value_ = orig_data[0];
    for (int i = 1; i < size; ++i) {
      if (orig_data[i] < min_value_)
        min_value_ = orig_data[i];
      else if (orig_data[i] > max_value_)
        max_value_ = orig_data[i];
    }
    InitCorrectionBounds();
    clamped_value_.resize(num_components);
  }

  void InitializeDecoding(int num_components) {
    PredictionSchemeTransform<DataTypeT, CorrTypeT>::InitializeDecoding(
        num_components);
    clamped_value_.resize(num_components);
  }

  // Computes the corrections based on the input original value and the
  // predicted value. Out of bound correction values are wrapped around the max
  // range of input values.
  inline void ComputeCorrection(const DataTypeT *original_vals,
                                const DataTypeT *predicted_vals,
                                CorrTypeT *out_corr_vals, int val_id) {
    PredictionSchemeTransform<DataTypeT, CorrTypeT>::ComputeCorrection(
        original_vals, ClampPredictedValue(predicted_vals), out_corr_vals,
        val_id);
    // Wrap around if needed.
    for (int i = 0; i < this->num_components(); ++i) {
      DataTypeT &corr_val = out_corr_vals[val_id + i];
      if (corr_val < min_correction_)
        corr_val += max_dif_;
      else if (corr_val > max_correction_)
        corr_val -= max_dif_;
    }
  }

  // Computes the original value from the input predicted value and the decoded
  // corrections. Values out of the bounds of the input values are unwrapped.
  inline void ComputeOriginalValue(const DataTypeT *predicted_vals,
                                   const CorrTypeT *corr_vals,
                                   DataTypeT *out_original_vals, int val_id) {
    PredictionSchemeTransform<DataTypeT, CorrTypeT>::ComputeOriginalValue(
        ClampPredictedValue(predicted_vals), corr_vals, out_original_vals,
        val_id);
    for (int i = 0; i < this->num_components(); ++i) {
      if (out_original_vals[i] > max_value_)
        out_original_vals[i] -= max_dif_;
      else if (out_original_vals[i] < min_value_)
        out_original_vals[i] += max_dif_;
    }
  }

  inline const DataTypeT *ClampPredictedValue(const DataTypeT *predicted_val) {
    for (int i = 0; i < this->num_components(); ++i) {
      if (predicted_val[i] > max_value_)
        clamped_value_[i] = max_value_;
      else if (predicted_val[i] < min_value_)
        clamped_value_[i] = min_value_;
      else
        clamped_value_[i] = predicted_val[i];
    }
    return &clamped_value_[0];
  }

  bool EncodeTransformData(EncoderBuffer *buffer) {
    // Store the input value range as it is needed by the decoder.
    buffer->Encode(min_value_);
    buffer->Encode(max_value_);
    return true;
  }

  bool DecodeTransformData(DecoderBuffer *buffer) {
    if (!buffer->Decode(&min_value_))
      return false;
    if (!buffer->Decode(&max_value_))
      return false;
    InitCorrectionBounds();
    return true;
  }

 protected:
  void InitCorrectionBounds() {
    max_dif_ = 1 + max_value_ - min_value_;
    max_correction_ = max_dif_ / 2;
    min_correction_ = -max_correction_;
    if ((max_dif_ & 1) == 0)
      max_correction_ -= 1;
  }

 private:
  DataTypeT min_value_;
  DataTypeT max_value_;
  DataTypeT max_dif_;
  DataTypeT max_correction_;
  DataTypeT min_correction_;
  std::vector<DataTypeT> clamped_value_;
};

}  // namespace draco

#endif  // DRACO_COMPRESSION_ATTRIBUTES_PREDICTION_SCHEMES_PREDICTION_SCHEME_WRAP_TRANSFORM_H_
