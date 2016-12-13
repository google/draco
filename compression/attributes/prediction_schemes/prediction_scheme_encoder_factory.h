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
// Functions for creating prediction schemes for encoders using the provided
// prediction method id.

#ifndef DRACO_COMPRESSION_ATTRIBUTES_PREDICTION_SCHEMES_PREDICTION_SCHEME_ENCODER_FACTORY_H_
#define DRACO_COMPRESSION_ATTRIBUTES_PREDICTION_SCHEMES_PREDICTION_SCHEME_ENCODER_FACTORY_H_

#include "compression/attributes/prediction_schemes/prediction_scheme_factory.h"
#include "compression/mesh/mesh_encoder.h"

namespace draco {

// Creates a prediction scheme for a given encoder and given prediction method.
// The prediction schemes are automatically initialized with encoder specific
// data if needed.
template <typename DataTypeT,
          class TransformT = PredictionSchemeTransform<DataTypeT, DataTypeT>>
std::unique_ptr<PredictionScheme<DataTypeT, TransformT>>
CreatePredictionSchemeForEncoder(PredictionSchemeMethod method, int att_id,
                                 const PointCloudEncoder *encoder,
                                 const TransformT &transform) {
  const PointAttribute *const att = encoder->point_cloud()->attribute(att_id);
  if (method == PREDICTION_UNDEFINED) {
    if (encoder->options()->GetSpeed() >= 10) {
      return nullptr;  // No prediction when the fastest speed is requested.
    }
    if (encoder->GetGeometryType() == TRIANGULAR_MESH) {
      // Use speed setting to select the best encoding method.
      if (encoder->options()->GetSpeed() >= 8) {
        method = PREDICTION_DIFFERENCE;
      } else if (encoder->options()->GetSpeed() >= 5) {
        method = MESH_PREDICTION_PARALLELOGRAM;
      } else {
        if (att->attribute_type() == GeometryAttribute::TEX_COORD) {
          method = MESH_PREDICTION_TEX_COORDS;
        } else {
          method = MESH_PREDICTION_MULTI_PARALLELOGRAM;
        }
      }
    }
  }
  if (encoder->GetGeometryType() == TRIANGULAR_MESH) {
    // Cast the encoder to mesh encoder. This is not necessarily safe if there
    // is some other encoder decides to use TRIANGULAR_MESH as the return type,
    // but unfortunately there is not nice work around for this without using
    // RTTI (double dispatch and similar conecepts will not work because of the
    // template nature of the prediction schemes).
    const MeshEncoder *const mesh_encoder =
        static_cast<const MeshEncoder *>(encoder);
    auto ret = CreateMeshPredictionScheme<MeshEncoder, DataTypeT, TransformT>(
        mesh_encoder, method, att_id, transform);
    if (ret)
      return ret;
    // Otherwise try to create another prediction scheme.
  }
  return CreatePredictionScheme<DataTypeT, TransformT>(method, att, transform);
}

// Create a prediction scheme using a default transform constructor.
template <typename DataTypeT,
          class TransformT = PredictionSchemeTransform<DataTypeT, DataTypeT>>
std::unique_ptr<PredictionScheme<DataTypeT, TransformT>>
CreatePredictionSchemeForEncoder(PredictionSchemeMethod method, int att_id,
                                 const PointCloudEncoder *encoder) {
  return CreatePredictionSchemeForEncoder<DataTypeT, TransformT>(
      method, att_id, encoder, TransformT());
}

// Returns the preferred prediction scheme based on the encoder options.
PredictionSchemeMethod GetPredictionMethodFromOptions(
    int att_id, const EncoderOptions &options);

}  // namespace draco

#endif  // DRACO_COMPRESSION_ATTRIBUTES_PREDICTION_SCHEMES_PREDICTION_SCHEME_ENCODER_FACTORY_H_
