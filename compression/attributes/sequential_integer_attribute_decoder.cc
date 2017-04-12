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
#include "compression/attributes/sequential_integer_attribute_decoder.h"

#include "compression/attributes/prediction_schemes/prediction_scheme_decoder_factory.h"
#include "compression/attributes/prediction_schemes/prediction_scheme_wrap_transform.h"
#include "core/symbol_decoding.h"

namespace draco {

SequentialIntegerAttributeDecoder::SequentialIntegerAttributeDecoder() {}

bool SequentialIntegerAttributeDecoder::Initialize(PointCloudDecoder *decoder,
                                                   int attribute_id) {
  if (!SequentialAttributeDecoder::Initialize(decoder, attribute_id))
    return false;
  return true;
}

bool SequentialIntegerAttributeDecoder::DecodeValues(
    const std::vector<PointIndex> &point_ids, DecoderBuffer *in_buffer) {
  const int32_t num_values = point_ids.size();
  // Decode prediction scheme.
  int8_t prediction_scheme_method;
  in_buffer->Decode(&prediction_scheme_method);
  if (prediction_scheme_method != PREDICTION_NONE) {
    int8_t prediction_transform_type;
    in_buffer->Decode(&prediction_transform_type);
    prediction_scheme_ = CreateIntPredictionScheme(
        static_cast<PredictionSchemeMethod>(prediction_scheme_method),
        static_cast<PredictionSchemeTransformType>(prediction_transform_type));
  }

  if (prediction_scheme_) {
    if (!InitPredictionScheme(prediction_scheme_.get()))
      return false;
  }

  if (!DecodeIntegerValues(point_ids, in_buffer))
    return false;

  if (!StoreValues(num_values))
    return false;
  return true;
}

std::unique_ptr<PredictionSchemeTypedInterface<int32_t>>
SequentialIntegerAttributeDecoder::CreateIntPredictionScheme(
    PredictionSchemeMethod method,
    PredictionSchemeTransformType transform_type) {
  if (transform_type != PREDICTION_TRANSFORM_WRAP)
    return nullptr;  // For now we support only wrap transform.
  return CreatePredictionSchemeForDecoder<
      int32_t, PredictionSchemeWrapTransform<int32_t>>(method, attribute_id(),
                                                       decoder());
}

bool SequentialIntegerAttributeDecoder::DecodeIntegerValues(
    const std::vector<PointIndex> &point_ids, DecoderBuffer *in_buffer) {
  const int num_components = GetNumValueComponents();
  if (num_components <= 0)
    return false;
  const int32_t num_values = point_ids.size();
  values_.resize(num_values * num_components);
  uint8_t compressed;
  if (!in_buffer->Decode(&compressed))
    return false;
  if (compressed > 0) {
    // Decode compressed values.
    if (!DecodeSymbols(num_values * num_components, num_components, in_buffer,
                       reinterpret_cast<uint32_t *>(values_.data())))
      return false;
  } else {
    // Decode the integer data directly.
    // Get the number of bytes for a given entry.
    uint8_t num_bytes;
    if (!in_buffer->Decode(&num_bytes))
      return false;
    if (num_bytes == sizeof(decltype(values_)::value_type)) {
      if (!in_buffer->Decode(values_.data(), sizeof(int32_t) * values_.size()))
        return false;
    } else {
      for (uint32_t i = 0; i < values_.size(); ++i) {
        in_buffer->Decode(&values_[i], num_bytes);
      }
    }
  }

  if (!values_.empty() && (prediction_scheme_ == nullptr ||
                           !prediction_scheme_->AreCorrectionsPositive())) {
    // Convert the values back to the original signed format.
    ConvertSymbolsToSignedInts(
        reinterpret_cast<const uint32_t *>(values_.data()), values_.size(),
        &values_[0]);
  }

  // If the data was encoded with a prediction scheme, we must revert it.
  if (prediction_scheme_) {
    if (!prediction_scheme_->DecodePredictionData(in_buffer))
      return false;

    if (!values_.empty()) {
      if (!prediction_scheme_->Decode(values_.data(), &values_[0],
                                      values_.size(), num_components,
                                      point_ids.data())) {
        return false;
      }
    }
  }
  return true;
}

bool SequentialIntegerAttributeDecoder::StoreValues(uint32_t num_values) {
  switch (attribute()->data_type()) {
    case DT_UINT8:
      StoreTypedValues<uint8_t>(num_values);
      break;
    case DT_INT8:
      StoreTypedValues<int8_t>(num_values);
      break;
    case DT_UINT16:
      StoreTypedValues<uint16_t>(num_values);
      break;
    case DT_INT16:
      StoreTypedValues<int16_t>(num_values);
      break;
    case DT_UINT32:
      StoreTypedValues<uint32_t>(num_values);
      break;
    case DT_INT32:
      StoreTypedValues<int32_t>(num_values);
      break;
    default:
      return false;
  }
  return true;
}

template <typename AttributeTypeT>
void SequentialIntegerAttributeDecoder::StoreTypedValues(uint32_t num_values) {
  const int num_components = attribute()->components_count();
  const int entry_size = sizeof(AttributeTypeT) * num_components;
  const std::unique_ptr<AttributeTypeT[]> att_val(
      new AttributeTypeT[num_components]);
  int val_id = 0;
  int out_byte_pos = 0;
  for (uint32_t i = 0; i < num_values; ++i) {
    for (int c = 0; c < num_components; ++c) {
      const AttributeTypeT value =
          static_cast<AttributeTypeT>(values_[val_id++]);
      att_val[c] = value;
    }
    // Store the integer value into the attribute buffer.
    attribute()->buffer()->Write(out_byte_pos, att_val.get(), entry_size);
    out_byte_pos += entry_size;
  }
}

}  // namespace draco
