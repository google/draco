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
#include "compression/attributes/attributes_decoder.h"

namespace draco {

AttributesDecoder::AttributesDecoder()
    : point_cloud_decoder_(nullptr), point_cloud_(nullptr) {}

bool AttributesDecoder::Initialize(PointCloudDecoder *decoder, PointCloud *pc) {
  point_cloud_decoder_ = decoder;
  point_cloud_ = pc;
  return true;
}

bool AttributesDecoder::DecodeAttributesDecoderData(DecoderBuffer *in_buffer) {
  // Decode and create attributes.
  int32_t num_attributes;
  if (!in_buffer->Decode(&num_attributes) || num_attributes <= 0)
    return false;
  point_attribute_ids_.resize(num_attributes);
  PointCloud *pc = point_cloud_;
  for (int i = 0; i < num_attributes; ++i) {
    // Decode attribute descriptor data.
    uint8_t att_type, data_type, components_count, normalized;
    uint16_t custom_id;
    if (!in_buffer->Decode(&att_type))
      return false;
    if (!in_buffer->Decode(&data_type))
      return false;
    if (!in_buffer->Decode(&components_count))
      return false;
    if (!in_buffer->Decode(&normalized))
      return false;
    if (!in_buffer->Decode(&custom_id))
      return false;
    const DataType draco_dt = static_cast<DataType>(data_type);

    // Add the attribute to the point cloud
    GeometryAttribute ga;
    ga.Init(static_cast<GeometryAttribute::Type>(att_type), nullptr,
            components_count, draco_dt, normalized > 0,
            DataTypeLength(draco_dt) * components_count, 0);
    ga.set_custom_id(custom_id);
    const int att_id = pc->AddAttribute(
        std::unique_ptr<PointAttribute>(new PointAttribute(ga)));
    point_attribute_ids_[i] = att_id;
  }
  return true;
}

}  // namespace draco
