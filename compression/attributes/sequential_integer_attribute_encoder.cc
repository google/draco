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
#include "compression/attributes/sequential_integer_attribute_encoder.h"

#include "compression/attributes/prediction_schemes/prediction_scheme_encoder_factory.h"
#include "compression/attributes/prediction_schemes/prediction_scheme_wrap_transform.h"
#include "core/bit_utils.h"
#include "core/symbol_encoding.h"

namespace draco {

SequentialIntegerAttributeEncoder::SequentialIntegerAttributeEncoder() {}

bool SequentialIntegerAttributeEncoder::Initialize(PointCloudEncoder *encoder,
                                                   int attribute_id) {
  if (!SequentialAttributeEncoder::Initialize(encoder, attribute_id))
    return false;
  if (GetUniqueId() == SEQUENTIAL_ATTRIBUTE_ENCODER_INTEGER) {
    // When encoding integers, this encoder currently works only for integer
    // attributes up to 32 bits.
    switch (attribute()->data_type()) {
      case DT_INT8:
      case DT_UINT8:
      case DT_INT16:
      case DT_UINT16:
      case DT_INT32:
      case DT_UINT32:
        break;
      default:
        return false;
    }
  }
  // Init prediction scheme.
  const PredictionSchemeMethod prediction_scheme_method =
      GetPredictionMethodFromOptions(attribute_id, *encoder->options());

  prediction_scheme_ = CreateIntPredictionScheme(prediction_scheme_method);

  if (prediction_scheme_ && !InitPredictionScheme(prediction_scheme_.get())) {
    prediction_scheme_ = nullptr;
  }

  return true;
}

std::unique_ptr<PredictionSchemeTypedInterface<int32_t>>
SequentialIntegerAttributeEncoder::CreateIntPredictionScheme(
    PredictionSchemeMethod method) {
  return CreatePredictionSchemeForEncoder<
      int32_t, PredictionSchemeWrapTransform<int32_t>>(method, attribute_id(),
                                                       encoder());
}

bool SequentialIntegerAttributeEncoder::EncodeValues(
    const std::vector<PointIndex> &point_ids, EncoderBuffer *out_buffer) {
  // Initialize general quantization data.
  const PointAttribute *const attrib = attribute();
  if (attrib->size() == 0)
    return true;

  int8_t prediction_scheme_method = PREDICTION_NONE;
  if (prediction_scheme_)
    prediction_scheme_method =
        static_cast<int8_t>(prediction_scheme_->GetPredictionMethod());
  out_buffer->Encode(prediction_scheme_method);
  if (prediction_scheme_) {
    out_buffer->Encode(
        static_cast<int8_t>(prediction_scheme_->GetTransformType()));
  }

  if (!PrepareValues(point_ids))
    return false;

  const int num_components = values_.size() / point_ids.size();
  // All integer values are initialized. Process them using the prediction
  // scheme if we have one.
  if (prediction_scheme_) {
    prediction_scheme_->Encode(values_.data(), &values_[0], values_.size(),
                               num_components, point_ids.data());
  }

  if (prediction_scheme_ == nullptr ||
      !prediction_scheme_->AreCorrectionsPositive()) {
    ConvertSignedIntsToSymbols(values_.data(), values_.size(),
                               reinterpret_cast<uint32_t *>(values_.data()));
  }

  if (encoder() == nullptr ||
      encoder()->options()->GetGlobalBool("use_built_in_attribute_compression",
                                          true)) {
    out_buffer->Encode(static_cast<uint8_t>(1));
    EncodeSymbols(reinterpret_cast<uint32_t *>(values_.data()),
                  point_ids.size() * num_components, num_components,
                  out_buffer);
  } else {
    // No compression. Just store the raw integer values, using the number of
    // bytes as needed.

    // To compute the maximum bit-length, first OR all values.
    uint32_t masked_value = 0;
    for (uint32_t i = 0; i < values_.size(); ++i) {
      masked_value |= values_[i];
    }
    // Compute the msb of the ORed value.
    int value_msb_pos = 0;
    if (masked_value != 0) {
      value_msb_pos = bits::MostSignificantBit(masked_value);
    }
    const int num_bytes = 1 + value_msb_pos / 8;

    out_buffer->Encode(static_cast<uint8_t>(0));
    out_buffer->Encode(static_cast<uint8_t>(num_bytes));

    if (num_bytes == sizeof(decltype(values_)::value_type)) {
      out_buffer->Encode(values_.data(), sizeof(int32_t) * values_.size());
    } else {
      for (uint32_t i = 0; i < values_.size(); ++i) {
        out_buffer->Encode(&values_[i], num_bytes);
      }
    }
  }
  if (prediction_scheme_) {
    prediction_scheme_->EncodePredictionData(out_buffer);
  }
  return true;
}

bool SequentialIntegerAttributeEncoder::PrepareValues(
    const std::vector<PointIndex> &point_ids) {
  // Convert all values to int32_t format.
  const PointAttribute *const attrib = attribute();
  const int num_components = attrib->components_count();
  const int num_entries = point_ids.size();
  values_.resize(num_entries * num_components);
  int dst_index = 0;
  for (int i = 0; i < num_entries; ++i) {
    const AttributeValueIndex att_id = attrib->mapped_index(point_ids[i]);
    attrib->ConvertValue<int32_t>(att_id, &values_[dst_index]);
    dst_index += num_components;
  }
  return true;
}

}  // namespace draco
