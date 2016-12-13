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
// Functions for creating prediction schemes from a provided prediction method
// name. The functions in this file can create only basic prediction schemes
// that don't require any encoder or decoder specific data. To create more
// sophisticated prediction schemes, use functions from either
// prediction_scheme_encoder_factory.h  or,
// prediction_scheme_decoder_factory.h.

#ifndef DRACO_COMPRESSION_ATTRIBUTES_PREDICTION_SCHEMES_PREDICTION_SCHEME_FACTORY_H_
#define DRACO_COMPRESSION_ATTRIBUTES_PREDICTION_SCHEMES_PREDICTION_SCHEME_FACTORY_H_

#include "compression/attributes/mesh_attribute_indices_encoding_data.h"
#include "compression/attributes/prediction_schemes/mesh_prediction_scheme_multi_parallelogram.h"
#include "compression/attributes/prediction_schemes/mesh_prediction_scheme_parallelogram.h"
#include "compression/attributes/prediction_schemes/mesh_prediction_scheme_tex_coords.h"
#include "compression/attributes/prediction_schemes/prediction_scheme_difference.h"
#include "compression/config/compression_shared.h"
#include "mesh/mesh_attribute_corner_table.h"

namespace draco {

template <typename DataTypeT,
          class TransformT = PredictionSchemeTransform<DataTypeT, DataTypeT>>
std::unique_ptr<PredictionScheme<DataTypeT, TransformT>> CreatePredictionScheme(
    PredictionSchemeMethod method, const PointAttribute *attribute,
    const TransformT &transform) {
  if (method == PREDICTION_NONE) {
    return nullptr;
  }
  return std::unique_ptr<PredictionScheme<DataTypeT, TransformT>>(
      new PredictionSchemeDifference<DataTypeT, TransformT>(attribute,
                                                            transform));
}

template <typename DataTypeT, class TransformT, class MeshDataT>
std::unique_ptr<PredictionScheme<DataTypeT, TransformT>>
CreateMeshPredictionSchemeInternal(PredictionSchemeMethod method,
                                   const PointAttribute *attribute,
                                   const TransformT &transform,
                                   const MeshDataT &mesh_data) {
  if (method == MESH_PREDICTION_PARALLELOGRAM) {
    return std::unique_ptr<PredictionScheme<DataTypeT, TransformT>>(
        new MeshPredictionSchemeParallelogram<DataTypeT, TransformT, MeshDataT>(
            attribute, transform, mesh_data));
  } else if (method == MESH_PREDICTION_MULTI_PARALLELOGRAM) {
    return std::unique_ptr<PredictionScheme<DataTypeT, TransformT>>(
        new MeshPredictionSchemeMultiParallelogram<DataTypeT, TransformT,
                                                   MeshDataT>(
            attribute, transform, mesh_data));
  } else if (method == MESH_PREDICTION_TEX_COORDS) {
    return std::unique_ptr<PredictionScheme<DataTypeT, TransformT>>(
        new MeshPredictionSchemeTexCoords<DataTypeT, TransformT, MeshDataT>(
            attribute, transform, mesh_data));
  }
  return nullptr;
}

template <class EncodingDataSourceT, typename DataTypeT, class TransformT>
std::unique_ptr<PredictionScheme<DataTypeT, TransformT>>
CreateMeshPredictionScheme(const EncodingDataSourceT *source,
                           PredictionSchemeMethod method, int att_id,
                           const TransformT &transform) {
  const PointAttribute *const att = source->point_cloud()->attribute(att_id);
  if (source->GetGeometryType() == TRIANGULAR_MESH &&
      (method == MESH_PREDICTION_PARALLELOGRAM ||
       method == MESH_PREDICTION_MULTI_PARALLELOGRAM ||
       method == MESH_PREDICTION_TEX_COORDS)) {
    const CornerTable *const ct = source->GetCornerTable();
    const MeshAttributeIndicesEncodingData *const encoding_data =
        source->GetAttributeEncodingData(att_id);
    if (ct == nullptr || encoding_data == nullptr) {
      // No connectivity data found.
      return nullptr;
    }
    // Connectivity data exists.
    const MeshAttributeCornerTable *const att_ct =
        source->GetAttributeCornerTable(att_id);
    if (att_ct != nullptr) {
      typedef MeshPredictionSchemeData<MeshAttributeCornerTable> MeshData;
      MeshData md;
      md.Set(source->mesh(), att_ct,
             &encoding_data->encoded_attribute_value_index_to_corner_map,
             &encoding_data->vertex_to_encoded_attribute_value_index_map);
      auto ret =
          CreateMeshPredictionSchemeInternal<DataTypeT, TransformT, MeshData>(
              method, att, transform, md);
      if (ret)
        return ret;
    } else {
      typedef MeshPredictionSchemeData<CornerTable> MeshData;
      MeshData md;
      md.Set(source->mesh(), ct,
             &encoding_data->encoded_attribute_value_index_to_corner_map,
             &encoding_data->vertex_to_encoded_attribute_value_index_map);
      auto ret =
          CreateMeshPredictionSchemeInternal<DataTypeT, TransformT, MeshData>(
              method, att, transform, md);
      if (ret)
        return ret;
    }
  }
  return nullptr;
}

}  // namespace draco

#endif  // DRACO_COMPRESSION_ATTRIBUTES_PREDICTION_SCHEMES_PREDICTION_SCHEME_FACTORY_H_
