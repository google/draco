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
#include "compression/attributes/sequential_quantization_attribute_encoder.h"

#include "core/quantization_utils.h"

namespace draco {

SequentialQuantizationAttributeEncoder::SequentialQuantizationAttributeEncoder()
    : max_value_dif_(0.f) {}

bool SequentialQuantizationAttributeEncoder::Initialize(
    PointCloudEncoder *encoder, int attribute_id) {
  if (!SequentialIntegerAttributeEncoder::Initialize(encoder, attribute_id))
    return false;
  // This encoder currently works only for floating point attributes.
  const PointAttribute *const attribute =
      encoder->point_cloud()->attribute(attribute_id);
  if (attribute->data_type() != DT_FLOAT32)
    return false;
  min_value_ = nullptr;
  return true;
}

bool SequentialQuantizationAttributeEncoder::PrepareValues(
    const std::vector<PointIndex> &point_ids) {
  if (!QuantizeValues(point_ids))
    return false;
  return true;
}

bool SequentialQuantizationAttributeEncoder::PrepareLossyAttributeData() {
  const int quantization_bits = encoder()->options()->GetAttributeInt(
      attribute_id(), "quantization_bits", -1);
  ComputeQuantizationData();
  const uint32_t max_quantized_value = (1 << (quantization_bits)) - 1;
  const PointAttribute *const attrib = attribute();
  const int num_components = attrib->components_count();
  const std::unique_ptr<float[]> att_val(new float[num_components]);

  PointAttribute *const lossy_attrib = encoded_lossy_attribute_data();

  Quantizer quantizer;
  quantizer.Init(max_value_dif_, max_quantized_value);
  Dequantizer dequantizer;
  dequantizer.Init(max_value_dif_, max_quantized_value);
  for (AttributeValueIndex i(0); i < attrib->size(); ++i) {
    attrib->GetValue(i, att_val.get());
    for (int c = 0; c < num_components; ++c) {
      float value = (att_val[c] - min_value_[c]);
      const int32_t q_val = quantizer.QuantizeFloat(value);
      value = dequantizer.DequantizeFloat(q_val) + min_value_[c];
      att_val[c] = value;
    }
    const int64_t out_byte_pos = lossy_attrib->GetBytePos(i);
    lossy_attrib->buffer()->Write(out_byte_pos, att_val.get(),
                                  lossy_attrib->byte_stride());
  }
  return true;
}

void SequentialQuantizationAttributeEncoder::ComputeQuantizationData() {
  if (min_value_ != nullptr)
    return;  // Already initialized.
  const PointAttribute *const attrib = attribute();
  const int num_components = attrib->components_count();
  max_value_dif_ = 0.f;
  min_value_ = std::unique_ptr<float[]>(new float[num_components]);
  const std::unique_ptr<float[]> max_value(new float[num_components]);
  const std::unique_ptr<float[]> att_val(new float[num_components]);
  // Compute minimum values and max value difference.
  attrib->GetValue(AttributeValueIndex(0), att_val.get());
  attrib->GetValue(AttributeValueIndex(0), min_value_.get());
  attrib->GetValue(AttributeValueIndex(0), max_value.get());

  for (AttributeValueIndex i(1); i < attrib->size(); ++i) {
    attrib->GetValue(i, att_val.get());
    for (int c = 0; c < num_components; ++c) {
      if (min_value_[c] > att_val[c])
        min_value_[c] = att_val[c];
      if (max_value[c] < att_val[c])
        max_value[c] = att_val[c];
    }
  }
  for (int c = 0; c < num_components; ++c) {
    const float dif = max_value[c] - min_value_[c];
    if (dif > max_value_dif_)
      max_value_dif_ = dif;
  }
  return;
}

bool SequentialQuantizationAttributeEncoder::QuantizeValues(
    const std::vector<PointIndex> &point_ids) {
  const int quantization_bits = encoder()->options()->GetAttributeInt(
      attribute_id(), "quantization_bits", -1);
  if (quantization_bits < 1)
    return false;

  const PointAttribute *const attrib = attribute();
  const int num_components = attrib->components_count();
  const std::unique_ptr<float[]> att_val(new float[num_components]);

  ComputeQuantizationData();

  encoder()->buffer()->Encode(min_value_.get(), sizeof(float) * num_components);
  encoder()->buffer()->Encode(max_value_dif_);
  encoder()->buffer()->Encode(static_cast<uint8_t>(quantization_bits));

  // Quantize all encoded values.
  values()->clear();
  values()->reserve(point_ids.size() * num_components);
  const uint32_t max_quantized_value = (1 << (quantization_bits)) - 1;
  Quantizer quantizer;
  quantizer.Init(max_value_dif_, max_quantized_value);
  for (int i = 0; i < point_ids.size(); ++i) {
    const AttributeValueIndex att_id = attrib->mapped_index(point_ids[i]);
    attribute()->GetValue(att_id, att_val.get());
    for (int c = 0; c < num_components; ++c) {
      const float value = (att_val[c] - min_value_[c]);
      const int32_t q_val = quantizer.QuantizeFloat(value);
      values()->push_back(q_val);
    }
  }
  return true;
}

}  // namespace draco
