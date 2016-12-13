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
#include "compression/attributes/sequential_attribute_encoder.h"

namespace draco {

SequentialAttributeEncoder::SequentialAttributeEncoder()
    : encoder_(nullptr),
      attribute_(nullptr),
      attribute_id_(-1),
      is_parent_encoder_(false) {}

bool SequentialAttributeEncoder::Initialize(PointCloudEncoder *encoder,
                                            int attribute_id) {
  encoder_ = encoder;
  attribute_ = encoder_->point_cloud()->attribute(attribute_id);
  attribute_id_ = attribute_id;
  return true;
}

bool SequentialAttributeEncoder::InitializeStandalone(
    PointAttribute *attribute) {
  attribute_ = attribute;
  attribute_id_ = -1;
  return true;
}

bool SequentialAttributeEncoder::Encode(
    const std::vector<PointIndex> &point_ids, EncoderBuffer *out_buffer) {
  if (!EncodeValues(point_ids, out_buffer))
    return false;
  if (is_parent_encoder_ && IsLossyEncoder()) {
    if (!PrepareLossyAttributeData())
      return false;
  }
  return true;
}

bool SequentialAttributeEncoder::EncodeValues(
    const std::vector<PointIndex> &point_ids, EncoderBuffer *out_buffer) {
  const int entry_size = attribute_->byte_stride();
  const std::unique_ptr<uint8_t[]> value_data_ptr(new uint8_t[entry_size]);
  uint8_t *const value_data = value_data_ptr.get();
  // Encode all attribute values in their native raw format.
  for (int i = 0; i < point_ids.size(); ++i) {
    const AttributeValueIndex entry_id = attribute_->mapped_index(point_ids[i]);
    attribute_->GetValue(entry_id, value_data);
    out_buffer->Encode(value_data, entry_size);
  }
  return true;
}

void SequentialAttributeEncoder::MarkParentAttribute() {
  is_parent_encoder_ = true;
  if (IsLossyEncoder() && encoded_lossy_attribute_data_ == nullptr) {
    // Prepare a new attribute that is going to store encoded lossy attribute
    // data that can be used by other attribute encoders. The attribute type,
    // size and other properties are equivalent to the original attribute, but
    // the stored values will be different, representing the data that is
    // going to be used by the decoder.
    GeometryAttribute va;
    va.Init(attribute_->attribute_type(), nullptr,
            attribute_->components_count(), attribute_->data_type(),
            attribute_->normalized(), attribute_->byte_stride(),
            attribute_->byte_offset());
    encoded_lossy_attribute_data_ =
        std::unique_ptr<PointAttribute>(new PointAttribute(va));
    encoded_lossy_attribute_data_->Reset(attribute_->size());
    // Set the correct point to value entry mapping.
    if (attribute_->is_mapping_identity()) {
      encoded_lossy_attribute_data_->SetIdentityMapping();
    } else {
      const int32_t num_points = encoder_->point_cloud()->num_points();
      encoded_lossy_attribute_data_->SetExplicitMapping(num_points);
      for (PointIndex i(0); i < num_points; ++i) {
        encoded_lossy_attribute_data_->SetPointMapEntry(
            i, attribute_->mapped_index(i));
      }
    }
  }
}

bool SequentialAttributeEncoder::InitPredictionScheme(
    PredictionSchemeInterface *ps) {
  for (int i = 0; i < ps->GetNumParentAttributes(); ++i) {
    const int att_id = encoder_->point_cloud()->GetNamedAttributeId(
        ps->GetParentAttributeType(i));
    if (att_id == -1)
      return false;  // Requested attribute does not exist.
    parent_attributes_.push_back(att_id);
    encoder_->MarkParentAttribute(att_id);
    if (!ps->SetParentAttribute(encoder_->GetLossyAttributeData(att_id)))
      return false;
  }
  return true;
}

bool SequentialAttributeEncoder::PrepareLossyAttributeData() {
  DCHECK(false);  // Method not implemented for a lossy encoder.
  return false;
}

}  // namespace draco
