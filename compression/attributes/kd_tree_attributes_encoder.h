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
#ifndef DRACO_COMPRESSION_ATTRIBUTES_POINT_CLOUD_KD_TREE_ATTRIBUTES_ENCODER_H_
#define DRACO_COMPRESSION_ATTRIBUTES_POINT_CLOUD_KD_TREE_ATTRIBUTES_ENCODER_H_

#include "compression/attributes/attributes_encoder.h"
#include "compression/config/compression_shared.h"

namespace draco {

// Encodes all attributes of a given PointCloud using one of the available
// Kd-tree compression methods.
// See compression/point_cloud/point_cloud_kd_tree_encoder.h for more details.
class KdTreeAttributesEncoder : public AttributesEncoder {
 public:
  KdTreeAttributesEncoder();
  explicit KdTreeAttributesEncoder(int att_id);

  uint8_t GetUniqueId() const override { return KD_TREE_ATTRIBUTE_ENCODER; }
  bool EncodeAttributes(EncoderBuffer *out_buffer) override;
};

}  // namespace draco

#endif  // DRACO_COMPRESSION_ATTRIBUTES_POINT_CLOUD_KD_TREE_ATTRIBUTES_ENCODER_H_
