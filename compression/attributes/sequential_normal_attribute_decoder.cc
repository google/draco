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
#include "compression/attributes/sequential_normal_attribute_decoder.h"
#include "compression/attributes/normal_compression_utils.h"

namespace draco {

SequentialNormalAttributeDecoder::SequentialNormalAttributeDecoder()
    : quantization_bits_(-1) {}

bool SequentialNormalAttributeDecoder::Initialize(PointCloudDecoder *decoder,
                                                  int attribute_id) {
  if (!SequentialIntegerAttributeDecoder::Initialize(decoder, attribute_id))
    return false;
  // Currently, this encoder works only for 3-component normal vectors.
  if (attribute()->components_count() != 3)
    return false;
  // Also the data type must be DT_FLOAT32.
  if (attribute()->data_type() != DT_FLOAT32)
    return false;
  return true;
}

bool SequentialNormalAttributeDecoder::DecodeIntegerValues(
    const std::vector<PointIndex> &point_ids, DecoderBuffer *in_buffer) {
  uint8_t quantization_bits;
  if (!in_buffer->Decode(&quantization_bits))
    return false;
  quantization_bits_ = quantization_bits;
  return SequentialIntegerAttributeDecoder::DecodeIntegerValues(point_ids,
                                                                in_buffer);
}

bool SequentialNormalAttributeDecoder::StoreValues(uint32_t num_points) {
  // Convert all quantized values back to floats.
  const int32_t max_quantized_value = (1 << quantization_bits_) - 1;
  const float max_quantized_value_f = static_cast<float>(max_quantized_value);

  const int num_components = attribute()->components_count();
  const int entry_size = sizeof(float) * num_components;
  float att_val[3];
  int quant_val_id = 0;
  int out_byte_pos = 0;
  for (uint32_t i = 0; i < num_points; ++i) {
    const int32_t s = values()->at(quant_val_id++);
    const int32_t t = values()->at(quant_val_id++);
    QuantizedOctaherdalCoordsToUnitVector(s, t, max_quantized_value_f, att_val);
    // Store the decoded floating point value into the attribute buffer.
    attribute()->buffer()->Write(out_byte_pos, att_val, entry_size);
    out_byte_pos += entry_size;
  }
  return true;
}

}  // namespace draco
