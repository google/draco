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
#ifndef DRACO_COMPRESSION_ATTRIBUTES_PREDICTION_SCHEMES_PREDICTION_SCHEME_DIFFERENCE_H_
#define DRACO_COMPRESSION_ATTRIBUTES_PREDICTION_SCHEMES_PREDICTION_SCHEME_DIFFERENCE_H_

#include "compression/attributes/prediction_schemes/prediction_scheme.h"

namespace draco {

// Basic prediction scheme based on computing backward differences between
// stored attribute values (also known as delta-coding). Usually works better
// than the reference point prediction scheme, because nearby values are often
// encoded next to each other.
template <typename DataTypeT,
          class Transform = PredictionSchemeTransform<DataTypeT, DataTypeT>>
class PredictionSchemeDifference
    : public PredictionScheme<DataTypeT, Transform> {
 public:
  using CorrType = typename PredictionScheme<DataTypeT, Transform>::CorrType;
  // Initialized the prediction scheme.
  explicit PredictionSchemeDifference(const PointAttribute *attribute)
      : PredictionScheme<DataTypeT, Transform>(attribute) {}
  PredictionSchemeDifference(const PointAttribute *attribute,
                             const Transform &transform)
      : PredictionScheme<DataTypeT, Transform>(attribute, transform) {}

  bool Encode(const DataTypeT *in_data, CorrType *out_corr, int size,
              int num_components,
              const PointIndex *entry_to_point_id_map) override;
  bool Decode(const CorrType *in_corr, DataTypeT *out_data, int size,
              int num_components,
              const PointIndex *entry_to_point_id_map) override;
  PredictionSchemeMethod GetPredictionMethod() const override {
    return PREDICTION_DIFFERENCE;
  }
  bool IsInitialized() const override { return true; }
};

template <typename DataTypeT, class Transform>
bool PredictionSchemeDifference<DataTypeT, Transform>::Encode(
    const DataTypeT *in_data, CorrType *out_corr, int size, int num_components,
    const PointIndex *) {
  this->transform().InitializeEncoding(in_data, size, num_components);
  // Encode data from the back using D(i) = D(i) - D(i - 1).
  for (int i = size - num_components; i > 0; i -= num_components) {
    this->transform().ComputeCorrection(
        in_data + i, in_data + i - num_components, out_corr, i);
  }
  // Encode correction for the first element.
  std::unique_ptr<DataTypeT[]> zero_vals(new DataTypeT[num_components]());
  this->transform().ComputeCorrection(in_data, zero_vals.get(), out_corr, 0);
  return true;
}

template <typename DataTypeT, class Transform>
bool PredictionSchemeDifference<DataTypeT, Transform>::Decode(
    const CorrType *in_corr, DataTypeT *out_data, int size, int num_components,
    const PointIndex *) {
  this->transform().InitializeDecoding(num_components);
  // Decode the original value for the first element.
  std::unique_ptr<DataTypeT[]> zero_vals(new DataTypeT[num_components]());
  this->transform().ComputeOriginalValue(zero_vals.get(), in_corr, out_data, 0);

  // Decode data from the front using D(i) = D(i) + D(i - 1).
  for (int i = num_components; i < size; i += num_components) {
    this->transform().ComputeOriginalValue(out_data + i - num_components,
                                           in_corr, out_data + i, i);
  }
  return true;
}

}  // namespace draco

#endif  // DRACO_COMPRESSION_ATTRIBUTES_PREDICTION_SCHEMES_PREDICTION_SCHEME_DIFFERENCE_H_
