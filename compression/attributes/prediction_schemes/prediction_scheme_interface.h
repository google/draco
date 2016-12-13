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
#ifndef DRACO_COMPRESSION_ATTRIBUTES_PREDICTION_SCHEMES_PREDICTION_SCHEME_INTERFACE_H_
#define DRACO_COMPRESSION_ATTRIBUTES_PREDICTION_SCHEMES_PREDICTION_SCHEME_INTERFACE_H_

#include "compression/config/compression_shared.h"
#include "core/decoder_buffer.h"
#include "core/encoder_buffer.h"
#include "point_cloud/point_attribute.h"

// Prediction schemes can be used during encoding and decoding of attributes
// to predict attribute values based on the previously encoded/decoded data.
// See prediction_scheme.h for more details.
namespace draco {

// Abstract interface for all prediction schemes used during attribute encoding.
class PredictionSchemeInterface {
 public:
  virtual ~PredictionSchemeInterface() = default;
  virtual PredictionSchemeMethod GetPredictionMethod() const = 0;

  // Returns the encoded attribute.
  virtual const PointAttribute *GetAttribute() const = 0;

  // Returns true when the prediction scheme is initialized with all data it
  // needs.
  virtual bool IsInitialized() const = 0;

  // Returns the number of parent attributes that are needed for the prediction.
  virtual int GetNumParentAttributes() const = 0;

  // Returns the type of each of the parent attribute.
  virtual GeometryAttribute::Type GetParentAttributeType(int i) const = 0;

  // Sets the required parent attribute.
  // Returns false if the attribute doesn't meet the requirements of the
  // prediction scheme.
  virtual bool SetParentAttribute(const PointAttribute *att) = 0;

  // Method that can be used to encode any prediction scheme specific data
  // into the output buffer.
  virtual bool EncodePredictionData(EncoderBuffer *buffer) = 0;

  // Method that can be used to decode any prediction scheme specific data
  // from the input buffer.
  virtual bool DecodePredictionData(DecoderBuffer *buffer) = 0;

  // Method should return true if the prediction scheme guarantees that all
  // correction values are always positive (or at least non-negative).
  virtual bool AreCorrectionsPositive() = 0;

  // Returns the transform type used by the prediction scheme.
  virtual PredictionSchemeTransformType GetTransformType() const = 0;
};

// A specialized version of the prediction scheme interface for specific
// input and output data types.
// |entry_to_point_id_map| is the mapping between value entries to point ids
// of the associated point cloud, where one entry is defined as |num_components|
// values of the |in_data|.
// DataTypeT is the data type of input and predicted values.
// CorrTypeT is the data type used for storing corrected values.
template <typename DataTypeT, typename CorrTypeT = DataTypeT>
class PredictionSchemeTypedInterface : public PredictionSchemeInterface {
 public:
  // Applies the prediction scheme when encoding the attribute.
  // |in_data| contains value entries to be encoded.
  // |out_corr| is an output array containing encoded corrections.
  virtual bool Encode(const DataTypeT *in_data, CorrTypeT *out_corr, int size,
                      int num_components,
                      const PointIndex *entry_to_point_id_map) = 0;

  // Reverts changes made by the prediction scheme during encoding.
  virtual bool Decode(const CorrTypeT *in_corr, DataTypeT *out_data, int size,
                      int num_components,
                      const PointIndex *entry_to_point_id_map) = 0;
};

}  // namespace draco

#endif  // DRACO_COMPRESSION_ATTRIBUTES_PREDICTION_SCHEMES_PREDICTION_SCHEME_INTERFACE_H_
