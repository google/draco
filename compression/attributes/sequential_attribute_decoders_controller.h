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
#ifndef DRACO_COMPRESSION_ATTRIBUTES_SEQUENTIAL_ATTRIBUTE_DECODERS_CONTROLLER_H_
#define DRACO_COMPRESSION_ATTRIBUTES_SEQUENTIAL_ATTRIBUTE_DECODERS_CONTROLLER_H_

#include "compression/attributes/attributes_decoder.h"
#include "compression/attributes/points_sequencer.h"
#include "compression/attributes/sequential_attribute_decoder.h"

namespace draco {

// A basic implementation of an attribute decoder that decodes data encoded by
// the SequentialAttributeEncodersController class. The
// SequentialAttributeDecodersController creates a single
// AttributeIndexedValuesDecoder for each of the decoded attribute, where the
// type of the values decoder is determined by the unique identifier that was
// encoded by the encoder.
class SequentialAttributeDecodersController : public AttributesDecoder {
 public:
  SequentialAttributeDecodersController(
      std::unique_ptr<PointsSequencer> sequencer);

  bool DecodeAttributesDecoderData(DecoderBuffer *buffer) override;
  bool DecodeAttributes(DecoderBuffer *buffer) override;

 protected:
  virtual std::unique_ptr<SequentialAttributeDecoder> CreateSequentialDecoder(
      uint8_t decoder_type);

 private:
  std::vector<std::unique_ptr<SequentialAttributeDecoder>> sequential_decoders_;
  std::vector<PointIndex> point_ids_;
  std::unique_ptr<PointsSequencer> sequencer_;
};

}  // namespace draco

#endif  // DRACO_COMPRESSION_ATTRIBUTES_SEQUENTIAL_ATTRIBUTE_DECODERS_CONTROLLER_H_
