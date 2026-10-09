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
#ifndef DRACO_COMPRESSION_ATTRIBUTES_PREDICTION_SCHEMES_PREDICTION_SCHEME_WRAP_DECODING_TRANSFORM_H_
#define DRACO_COMPRESSION_ATTRIBUTES_PREDICTION_SCHEMES_PREDICTION_SCHEME_WRAP_DECODING_TRANSFORM_H_

#include "draco/compression/attributes/prediction_schemes/prediction_scheme_wrap_transform_base.h"
#include "draco/core/decoder_buffer.h"

namespace draco {

// PredictionSchemeWrapDecodingTransform unwraps values encoded with the
// PredictionSchemeWrapEncodingTransform.
// See prediction_scheme_wrap_transform_base.h for more details about the
// method.
template <typename DataTypeT, typename CorrTypeT = DataTypeT>
class PredictionSchemeWrapDecodingTransform
    : public PredictionSchemeWrapTransformBase<DataTypeT> {
 public:
  typedef CorrTypeT CorrType;
  PredictionSchemeWrapDecodingTransform() {}

  // Computes the original value from the input predicted value and the decoded
  // corrections. Values out of the bounds of the input values are unwrapped.
  inline void ComputeOriginalValue(const DataTypeT *predicted_vals,
                                   const CorrTypeT *corr_vals,
                                   DataTypeT *out_original_vals) const {
    // For now we assume both |DataTypeT| and |CorrTypeT| are equal.
    static_assert(std::is_same<DataTypeT, CorrTypeT>::value,
                  "Predictions and corrections must have the same type.");

    // The only valid implementation right now is for int32_t.
    static_assert(std::is_same<DataTypeT, int32_t>::value,
                  "Only int32_t is supported for predicted values.");

    predicted_vals = this->ClampPredictedValue(predicted_vals);

    // The sum is computed in int64_t, where it is exact. The encoder keeps each
    // correction within half of the span, so the sum lies at most one span
    // outside [min, max] and a single wrap gives the original value. Malformed
    // corrections cannot overflow int64_t either.
    for (int i = 0; i < this->num_components(); ++i) {
      int64_t value = static_cast<int64_t>(predicted_vals[i]) +
                      static_cast<int64_t>(corr_vals[i]);
      if (value > this->max_value()) {
        value -= this->max_dif();
      } else if (value < this->min_value()) {
        value += this->max_dif();
      }
      out_original_vals[i] = static_cast<DataTypeT>(value);
    }
  }

  bool DecodeTransformData(DecoderBuffer *buffer) {
    DataTypeT min_value, max_value;
    if (!buffer->Decode(&min_value)) {
      return false;
    }
    if (!buffer->Decode(&max_value)) {
      return false;
    }
    if (min_value > max_value) {
      return false;
    }
    this->set_min_value(min_value);
    this->set_max_value(max_value);
    if (!this->InitCorrectionBounds()) {
      return false;
    }
    return true;
  }
};

}  // namespace draco

#endif  // DRACO_COMPRESSION_ATTRIBUTES_PREDICTION_SCHEMES_PREDICTION_SCHEME_WRAP_DECODING_TRANSFORM_H_
