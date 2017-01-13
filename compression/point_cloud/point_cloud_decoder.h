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
#ifndef DRACO_COMPRESSION_POINT_CLOUD_POINT_CLOUD_DECODER_H_
#define DRACO_COMPRESSION_POINT_CLOUD_POINT_CLOUD_DECODER_H_

#include "compression/attributes/attributes_decoder.h"
#include "compression/config/compression_shared.h"
#include "point_cloud/point_cloud.h"

namespace draco {

// Abstract base class for all point cloud and mesh decoders. It provides a
// basic funcionality that is shared between different decoders.
class PointCloudDecoder {
 public:
  PointCloudDecoder();
  virtual ~PointCloudDecoder() = default;

  virtual EncodedGeometryType GetGeometryType() const { return POINT_CLOUD; }

  // The main entry point for point cloud decoding.
  bool Decode(DecoderBuffer *in_buffer, PointCloud *out_point_cloud);

  void SetAttributesDecoder(int att_decoder_id,
                            std::unique_ptr<AttributesDecoder> decoder) {
    if (att_decoder_id >= static_cast<int>(attributes_decoders_.size()))
      attributes_decoders_.resize(att_decoder_id + 1);
    attributes_decoders_[att_decoder_id] = std::move(decoder);
  }
  const AttributesDecoder *attributes_decoder(int dec_id) {
    return attributes_decoders_[dec_id].get();
  }
  int32_t num_attributes_decoders() const {
    return attributes_decoders_.size();
  }

  // Get a mutable pointer to the decoded point cloud. This is intended to be
  // used mostly by other decoder subsystems.
  PointCloud *point_cloud() { return point_cloud_; }
  const PointCloud *point_cloud() const { return point_cloud_; }

  DecoderBuffer *buffer() { return buffer_; }

 protected:
  // Can be implemented by derived classes to perform any custom initialization
  // of the decoder. Called in the Decode() method.
  virtual bool InitializeDecoder() { return true; }

  // Creates an attribute decoder.
  virtual bool CreateAttributesDecoder(int32_t att_decoder_id) = 0;
  virtual bool DecodeGeometryData() { return true; }
  virtual bool DecodePointAttributes();

  virtual bool DecodeAllAttributes();
  virtual bool OnAttributesDecoded() { return true; }

 private:
  // Point cloud that is being filled in by the decoder.
  PointCloud *point_cloud_;

  std::vector<std::unique_ptr<AttributesDecoder>> attributes_decoders_;

  // Input buffer holding the encoded data.
  DecoderBuffer *buffer_;
};

}  // namespace draco

#endif  // DRACO_COMPRESSION_POINT_CLOUD_POINT_CLOUD_DECODER_H_
