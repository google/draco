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
#ifndef DRACO_COMPRESSION_ATTRIBUTES_SEQUENTIAL_QUANTIZATION_ATTRIBUTE_ENCODER_H_
#define DRACO_COMPRESSION_ATTRIBUTES_SEQUENTIAL_QUANTIZATION_ATTRIBUTE_ENCODER_H_

#include "compression/attributes/sequential_integer_attribute_encoder.h"

namespace draco {

class MeshEncoder;

// Attribute encoder that quantizes floating point attribute values. The
// quantized values can be optionally compressed using an entropy coding.
class SequentialQuantizationAttributeEncoder
    : public SequentialIntegerAttributeEncoder {
 public:
  SequentialQuantizationAttributeEncoder();
  uint8_t GetUniqueId() const override {
    return SEQUENTIAL_ATTRIBUTE_ENCODER_QUANTIZATION;
  }
  bool Initialize(PointCloudEncoder *encoder, int attribute_id) override;

  bool IsLossyEncoder() const override { return true; }

 protected:
  bool PrepareValues(const std::vector<PointIndex> &point_ids) override;
  bool PrepareLossyAttributeData() override;

  // Quantizes all values of the input attribute. Quantization behavior can be
  // changed by the derived classes.
  virtual bool QuantizeValues(const std::vector<PointIndex> &point_ids);

 private:
  // Computes quantization data when needed. The output is computed only once
  // even if the function is called multiple times (this can be reset only
  // by calling the Initialize() method).
  void ComputeQuantizationData();

  // Data that are needed by PrepareLossyAttributeData().
  float max_value_dif_;
  std::unique_ptr<float[]> min_value_;
};

}  // namespace draco

#endif  // DRACO_COMPRESSION_ATTRIBUTES_SEQUENTIAL_QUANTIZATION_ATTRIBUTE_ENCODER_H_
