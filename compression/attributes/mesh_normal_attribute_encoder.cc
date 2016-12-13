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
#include "compression/attributes/mesh_normal_attribute_encoder.h"
#include "compression/attributes/normal_compression_utils.h"

namespace draco {

bool MeshNormalAttributeEncoder::Initialize(PointCloudEncoder *encoder,
                                            int attribute_id) {
  if (!SequentialIntegerAttributeEncoder::Initialize(encoder, attribute_id))
    return false;
  // Currently this encoder works only for 3-component normal vectors.
  if (attribute()->components_count() != 3)
    return false;
  return true;
}

bool MeshNormalAttributeEncoder::PrepareValues(
    const std::vector<PointIndex> &point_ids) {
  // Quantize all encoded values.
  const int quantization_bits = encoder()->options()->GetAttributeInt(
      attribute_id(), "quantization_bits", -1);
  encoder()->buffer()->Encode(static_cast<uint8_t>(quantization_bits));

  const int32_t max_quantized_value = (1 << quantization_bits) - 1;
  const float max_quantized_value_f = static_cast<float>(max_quantized_value);
  values()->clear();
  float att_val[3];
  values()->reserve(point_ids.size() * 2);
  for (int i = 0; i < point_ids.size(); ++i) {
    const AttributeValueIndex att_id = attribute()->mapped_index(point_ids[i]);
    attribute()->GetValue(att_id, att_val);
    // Encode the vector into a s and t octaherdal coordinates.
    int32_t s, t;
    UnitVectorToQuantizedOctahedralCoords(att_val, max_quantized_value_f, &s,
                                          &t);
    values()->push_back(s);
    values()->push_back(t);
  }
  return true;
}

}  // namespace draco
