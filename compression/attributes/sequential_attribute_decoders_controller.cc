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
#include "compression/attributes/sequential_attribute_decoders_controller.h"
#include "compression/attributes/sequential_normal_attribute_decoder.h"
#include "compression/attributes/sequential_quantization_attribute_decoder.h"
#include "compression/config/compression_shared.h"

namespace draco {

SequentialAttributeDecodersController::SequentialAttributeDecodersController(
    std::unique_ptr<PointsSequencer> sequencer)
    : sequencer_(std::move(sequencer)) {}

bool SequentialAttributeDecodersController::DecodeAttributesDecoderData(
    DecoderBuffer *buffer) {
  if (!AttributesDecoder::DecodeAttributesDecoderData(buffer))
    return false;
  // Decode unique ids of all sequential encoders and create them.
  sequential_decoders_.resize(num_attributes());
  for (int i = 0; i < num_attributes(); ++i) {
    uint8_t decoder_type;
    if (!buffer->Decode(&decoder_type))
      return false;
    // Create the decoder from the id.
    sequential_decoders_[i] = CreateSequentialDecoder(decoder_type);
    if (!sequential_decoders_[i])
      return false;
    if (!sequential_decoders_[i]->Initialize(decoder(), GetAttributeId(i)))
      return false;
  }
  return true;
}

bool SequentialAttributeDecodersController::DecodeAttributes(
    DecoderBuffer *buffer) {
  if (!sequencer_ || !sequencer_->GenerateSequence(&point_ids_))
    return false;
  // Initialize point to attribute value mapping for all decoded attributes.
  for (int i = 0; i < num_attributes(); ++i) {
    PointAttribute *const pa =
        decoder()->point_cloud()->attribute(GetAttributeId(i));
    if (!sequencer_->UpdatePointToAttributeIndexMapping(pa))
      return false;
  }
  for (int i = 0; i < num_attributes(); ++i) {
    if (!sequential_decoders_[i]->Decode(point_ids_, buffer))
      return false;
  }
  return true;
}

std::unique_ptr<SequentialAttributeDecoder>
SequentialAttributeDecodersController::CreateSequentialDecoder(
    uint8_t decoder_type) {
  switch (decoder_type) {
    case SEQUENTIAL_ATTRIBUTE_ENCODER_GENERIC:
      return std::unique_ptr<SequentialAttributeDecoder>(
          new SequentialAttributeDecoder());
    case SEQUENTIAL_ATTRIBUTE_ENCODER_INTEGER:
      return std::unique_ptr<SequentialAttributeDecoder>(
          new SequentialIntegerAttributeDecoder());
    case SEQUENTIAL_ATTRIBUTE_ENCODER_QUANTIZATION:
      return std::unique_ptr<SequentialAttributeDecoder>(
          new SequentialQuantizationAttributeDecoder());
    case SEQUENTIAL_ATTRIBUTE_ENCODER_NORMALS:
      return std::unique_ptr<SequentialNormalAttributeDecoder>(
          new SequentialNormalAttributeDecoder());
    default:
      break;
  }
  // Unknown or unsupported decoder type.
  return nullptr;
}

}  // namespace draco
