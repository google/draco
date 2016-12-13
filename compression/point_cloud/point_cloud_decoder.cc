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
#include "compression/point_cloud/point_cloud_decoder.h"

namespace draco {

PointCloudDecoder::PointCloudDecoder()
    : point_cloud_(nullptr), buffer_(nullptr) {}

bool PointCloudDecoder::Decode(DecoderBuffer *in_buffer,
                               PointCloud *out_point_cloud) {
  buffer_ = in_buffer;
  point_cloud_ = out_point_cloud;
  if (!InitializeDecoder())
    return false;
  if (!DecodeGeometryData())
    return false;
  if (!DecodePointAttributes())
    return false;
  return true;
}

bool PointCloudDecoder::DecodePointAttributes() {
  uint8_t num_attributes_decoders;
  if (!buffer_->Decode(&num_attributes_decoders))
    return false;
  // Create all attribute decoders. This is implementation specific and the
  // derived classes can use any data encoded in the
  // PointCloudEncoder::EncodeAttributesEncoderIdentifier() call.
  for (int i = 0; i < num_attributes_decoders; ++i) {
    if (!CreateAttributesDecoder(i))
      return false;
  }

  // Initialize all attributes decoders. No data is decoded here.
  for (auto &att_dec : attributes_decoders_) {
    if (!att_dec->Initialize(this, point_cloud_))
      return false;
  }

  // Decode any data needed by the attribute decoders.
  for (int i = 0; i < num_attributes_decoders; ++i) {
    if (!attributes_decoders_[i]->DecodeAttributesDecoderData(buffer_))
      return false;
  }

  // Decode the actual attributes using the created attribute decoders.
  if (!DecodeAllAttributes())
    return false;

  if (!OnAttributesDecoded())
    return false;
  return true;
}

bool PointCloudDecoder::DecodeAllAttributes() {
  for (auto &att_dec : attributes_decoders_) {
    if (!att_dec->DecodeAttributes(buffer_))
      return false;
  }
  return true;
}

}  // namespace draco
