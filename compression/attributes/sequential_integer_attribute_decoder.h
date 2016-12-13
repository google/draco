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
#ifndef DRACO_COMPRESSION_ATTRIBUTES_SEQUENTIAL_INTEGER_ATTRIBUTE_DECODER_H_
#define DRACO_COMPRESSION_ATTRIBUTES_SEQUENTIAL_INTEGER_ATTRIBUTE_DECODER_H_

#include "compression/attributes/prediction_schemes/prediction_scheme.h"
#include "compression/attributes/sequential_attribute_decoder.h"

namespace draco {

// Decoder for attributes encoded with the SequentialIntegerAttributeEncoder.
class SequentialIntegerAttributeDecoder : public SequentialAttributeDecoder {
 public:
  SequentialIntegerAttributeDecoder();
  bool Initialize(PointCloudDecoder *decoder, int attribute_id) override;

 protected:
  bool DecodeValues(const std::vector<PointIndex> &point_ids,
                    DecoderBuffer *in_buffer) override;
  virtual bool DecodeIntegerValues(const std::vector<PointIndex> &point_ids,
                                   DecoderBuffer *in_buffer);

  // Returns a prediction scheme that should be used for decoding of the
  // integer values.
  virtual std::unique_ptr<PredictionSchemeTypedInterface<int32_t>>
  CreateIntPredictionScheme(PredictionSchemeMethod method,
                            PredictionSchemeTransformType transform_type);

  // Returns the number of integer attribute components. In general, this
  // can be different from the number of components of the input attribute.
  virtual int32_t GetNumValueComponents() const {
    return attribute()->components_count();
  }

  // Called after all integer values are decoded. The implmentation should
  // use this method to store the values into the attribute.
  virtual bool StoreValues(uint32_t num_values);

  std::vector<int32_t> *values() { return &values_; }

 private:
  // Stores decoded values into the attribute with a data type AttributeTypeT.
  template <typename AttributeTypeT>
  void StoreTypedValues(uint32_t num_values);

  // Storage for decoded integer values.
  std::vector<int32_t> values_;

  std::unique_ptr<PredictionSchemeTypedInterface<int32_t>> prediction_scheme_;
};

}  // namespace draco

#endif  // DRACO_COMPRESSION_ATTRIBUTES_SEQUENTIAL_INTEGER_ATTRIBUTE_DECODER_H_
