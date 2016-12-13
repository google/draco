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
#include "compression/attributes/sequential_attribute_decoder.h"

namespace draco {

SequentialAttributeDecoder::SequentialAttributeDecoder()
    : decoder_(nullptr), attribute_(nullptr), attribute_id_(-1) {}

bool SequentialAttributeDecoder::Initialize(PointCloudDecoder *decoder,
                                            int attribute_id) {
  decoder_ = decoder;
  attribute_ = decoder->point_cloud()->attribute(attribute_id);
  attribute_id_ = attribute_id;
  return true;
}

bool SequentialAttributeDecoder::InitializeStandalone(
    PointAttribute *attribute) {
  attribute_ = attribute;
  attribute_id_ = -1;
  return true;
}

bool SequentialAttributeDecoder::Decode(
    const std::vector<PointIndex> &point_ids, DecoderBuffer *in_buffer) {
  attribute_->Reset(point_ids.size());
  if (!DecodeValues(point_ids, in_buffer))
    return false;
  return true;
}

bool SequentialAttributeDecoder::InitPredictionScheme(
    PredictionSchemeInterface *ps) {
  for (int i = 0; i < ps->GetNumParentAttributes(); ++i) {
    const int att_id = decoder_->point_cloud()->GetNamedAttributeId(
        ps->GetParentAttributeType(i));
    if (att_id == -1)
      return false;  // Requested attribute does not exist.
    if (!ps->SetParentAttribute(decoder_->point_cloud()->attribute(att_id)))
      return false;
  }
  return true;
}

bool SequentialAttributeDecoder::DecodeValues(
    const std::vector<PointIndex> &point_ids, DecoderBuffer *in_buffer) {
  const int32_t num_values = point_ids.size();
  const int entry_size = attribute_->byte_stride();
  std::unique_ptr<uint8_t[]> value_data_ptr(new uint8_t[entry_size]);
  uint8_t *const value_data = value_data_ptr.get();
  int out_byte_pos = 0;
  // Decode raw attribute values in their original format.
  for (int i = 0; i < num_values; ++i) {
    if (!in_buffer->Decode(value_data, entry_size))
      return false;
    attribute_->buffer()->Write(out_byte_pos, value_data, entry_size);
    out_byte_pos += entry_size;
  }
  return true;
}

}  // namespace draco
