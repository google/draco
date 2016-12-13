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
#ifndef DRACO_COMPRESSION_ATTRIBUTES_ATTRIBUTES_DECODER_H_
#define DRACO_COMPRESSION_ATTRIBUTES_ATTRIBUTES_DECODER_H_

#include <vector>

#include "core/decoder_buffer.h"
#include "point_cloud/point_cloud.h"

namespace draco {

class PointCloudDecoder;

// Base class for decoding one or more attributes that were encoded with a
// matching AttributesEncoder. This base class provides only the basic interface
// that is used by the PointCloudDecoder. The actual encoding must be
// implemented in derived classes using the DecodeAttributes() method.
class AttributesDecoder {
 public:
  AttributesDecoder();
  virtual ~AttributesDecoder() = default;

  // Called after all attribute decoders are created. It can be used to perform
  // any custom initialization.
  virtual bool Initialize(PointCloudDecoder *decoder, PointCloud *pc);

  // Decodes any attribute decoder specific data from the |in_buffer|.
  virtual bool DecodeAttributesDecoderData(DecoderBuffer *in_buffer);

  // Decode attribute data from the source buffer. Needs to be implmented by the
  // derived classes.
  virtual bool DecodeAttributes(DecoderBuffer *in_buffer) = 0;

  int32_t GetAttributeId(int i) const { return point_attribute_ids_[i]; }
  int32_t num_attributes() const { return point_attribute_ids_.size(); }
  PointCloudDecoder *decoder() const { return point_cloud_decoder_; }

 private:
  // List of attribute ids that need to be decoded with this decoder.
  std::vector<int32_t> point_attribute_ids_;

  PointCloudDecoder *point_cloud_decoder_;
  PointCloud *point_cloud_;
};

}  // namespace draco

#endif  // DRACO_COMPRESSION_ATTRIBUTES_ATTRIBUTES_DECODER_H_
