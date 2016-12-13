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
// Functions for creating prediction schemes for decoders using the provided
// prediction method id.

#ifndef DRACO_COMPRESSION_ATTRIBUTES_PREDICTION_SCHEMES_PREDICTION_SCHEME_DECODER_FACTORY_H_
#define DRACO_COMPRESSION_ATTRIBUTES_PREDICTION_SCHEMES_PREDICTION_SCHEME_DECODER_FACTORY_H_

#include "compression/attributes/prediction_schemes/prediction_scheme_factory.h"
#include "compression/mesh/mesh_decoder.h"

namespace draco {

// Creates a prediction scheme for a given decoder and given prediction method.
// The prediction schemes are automatically initialized with decoder specific
// data if needed.
template <typename DataTypeT,
          class TransformT = PredictionSchemeTransform<DataTypeT, DataTypeT>>
std::unique_ptr<PredictionScheme<DataTypeT, TransformT>>
CreatePredictionSchemeForDecoder(PredictionSchemeMethod method, int att_id,
                                 const PointCloudDecoder *decoder,
                                 const TransformT &transform) {
  const PointAttribute *const att = decoder->point_cloud()->attribute(att_id);
  if (decoder->GetGeometryType() == TRIANGULAR_MESH) {
    // Cast the decoder to mesh decoder. This is not necessarily safe if there
    // is some other decoder decides to use TRIANGULAR_MESH as the return type,
    // but unfortunately there is not nice work around for this without using
    // RTTI (double dispatch and similar conecepts will not work because of the
    // template nature of the prediction schemes).
    const MeshDecoder *const mesh_decoder =
        static_cast<const MeshDecoder *>(decoder);
    auto ret = CreateMeshPredictionScheme<MeshDecoder, DataTypeT, TransformT>(
        mesh_decoder, method, att_id, transform);
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
CreatePredictionSchemeForDecoder(PredictionSchemeMethod method, int att_id,
                                 const PointCloudDecoder *decoder) {
  return CreatePredictionSchemeForDecoder<DataTypeT, TransformT>(
      method, att_id, decoder, TransformT());
}

}  // namespace draco

#endif  // DRACO_COMPRESSION_ATTRIBUTES_PREDICTION_SCHEMES_PREDICTION_SCHEME_DECODER_FACTORY_H_
