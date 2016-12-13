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
#include "compression/attributes/attributes_encoder.h"

namespace draco {

AttributesEncoder::AttributesEncoder()
    : point_cloud_encoder_(nullptr), point_cloud_(nullptr) {}

AttributesEncoder::AttributesEncoder(int att_id) : AttributesEncoder() {
  point_attribute_ids_.push_back(att_id);
}

bool AttributesEncoder::Initialize(PointCloudEncoder *encoder,
                                   const PointCloud *pc) {
  point_cloud_encoder_ = encoder;
  point_cloud_ = pc;
  return true;
}

bool AttributesEncoder::EncodeAttributesEncoderData(EncoderBuffer *out_buffer) {
  // Encode data about all attributes.
  out_buffer->Encode(num_attributes());
  for (int i = 0; i < num_attributes(); ++i) {
    const int32_t att_id = point_attribute_ids_[i];
    const PointAttribute *const pa = point_cloud_->attribute(att_id);
    out_buffer->Encode(static_cast<uint8_t>(pa->attribute_type()));
    out_buffer->Encode(static_cast<uint8_t>(pa->data_type()));
    out_buffer->Encode(static_cast<uint8_t>(pa->components_count()));
    out_buffer->Encode(static_cast<uint8_t>(pa->normalized()));
    out_buffer->Encode(static_cast<uint16_t>(pa->custom_id()));
  }
  return true;
}

}  // namespace draco
