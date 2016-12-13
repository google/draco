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
#ifndef DRACO_COMPRESSION_ATTRIBUTES_PREDICTION_SCHEMES_MESH_PREDICTION_SCHEME_H_
#define DRACO_COMPRESSION_ATTRIBUTES_PREDICTION_SCHEMES_MESH_PREDICTION_SCHEME_H_

#include "compression/attributes/prediction_schemes/mesh_prediction_scheme_data.h"
#include "compression/attributes/prediction_schemes/prediction_scheme.h"

namespace draco {

// Base class for all mesh prediction schemes that use the mesh connectivity
// data. |MeshDataT| can be any class that provides the same interface as the
// PredictionSchemeMeshData class.
template <typename DataTypeT, class TransformT, class MeshDataT>
class MeshPredictionScheme : public PredictionScheme<DataTypeT, TransformT> {
 public:
  typedef MeshDataT MeshData;
  MeshPredictionScheme(const PointAttribute *attribute,
                       const TransformT &transform, const MeshDataT &mesh_data)
      : PredictionScheme<DataTypeT, TransformT>(attribute, transform),
        mesh_data_(mesh_data) {}

 protected:
  const MeshData &mesh_data() const { return mesh_data_; }

 private:
  MeshData mesh_data_;
};

}  // namespace draco

#endif  // DRACO_COMPRESSION_ATTRIBUTES_PREDICTION_SCHEMES_MESH_PREDICTION_SCHEME_H_
