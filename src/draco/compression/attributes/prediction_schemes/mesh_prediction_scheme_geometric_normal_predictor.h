// Copyright 2017 The Draco Authors.
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
#ifndef DRACO_COMPRESSION_ATTRIBUTES_PREDICTION_SCHEMES_MESH_PREDICTION_SCHEME_GEOMETRIC_NORMAL_PREDICTOR_H_
#define DRACO_COMPRESSION_ATTRIBUTES_PREDICTION_SCHEMES_MESH_PREDICTION_SCHEME_GEOMETRIC_NORMAL_PREDICTOR_H_

#include <math.h>

#include "draco/attributes/point_attribute.h"
#include "draco/compression/attributes/normal_compression_utils.h"
#include "draco/core/math_utils.h"
#include "draco/core/vector_d.h"
#include "draco/mesh/corner_table.h"
#include "draco/mesh/corner_table_iterators.h"

namespace draco {

// This predictor estimates the normal via the surrounding triangles of the
// given corner. Triangles are weighted according to their area.
template <typename DataTypeT, class TransformT, class MeshDataT>
class MeshPredictionSchemeGeometricNormalPredictor {
 public:
  MeshPredictionSchemeGeometricNormalPredictor(const MeshDataT &md)
      : pos_attribute_(nullptr),
        entry_to_point_id_map_(nullptr),
        mesh_data_(md) {}
  void SetPositionAttribute(const PointAttribute &position_attribute) {
    pos_attribute_ = &position_attribute;
  }
  void SetEntryToPointIdMap(const PointIndex *map) {
    entry_to_point_id_map_ = map;
  }

  bool IsInitialized() const {
    if (pos_attribute_ == nullptr)
      return false;
    if (entry_to_point_id_map_ == nullptr)
      return false;
    return true;
  }

  VectorD<int64_t, 3> GetPositionForDataId(int data_id) const {
    DCHECK(this->IsInitialized());
    const auto point_id = entry_to_point_id_map_[data_id];
    const auto pos_val_id = pos_attribute_->mapped_index(point_id);
    VectorD<int64_t, 3> pos;
    pos_attribute_->ConvertValue(pos_val_id, &pos[0]);
    return pos;
  }
  VectorD<int64_t, 3> GetPositionForCorner(CornerIndex ci) const {
    DCHECK(this->IsInitialized());
    const auto corner_table = mesh_data_.corner_table();
    const auto vert_id = corner_table->Vertex(ci).value();
    const auto data_id = mesh_data_.vertex_to_data_map()->at(vert_id);
    return GetPositionForDataId(data_id);
  }

  VectorD<int32_t, 2> GetOctahedralCoordForDataId(int data_id,
                                                  const DataTypeT *data) const {
    DCHECK(this->IsInitialized());
    const int data_offset = data_id * 2;
    return VectorD<int32_t, 2>(data[data_offset], data[data_offset + 1]);
  }

  // Computes predicted octahedral coordinates on a given corner.
  void ComputePredictedValue(CornerIndex corner_id, DataTypeT *prediction) {
    DCHECK(this->IsInitialized());
    typedef typename MeshDataT::CornerTable CornerTable;
    const CornerTable *const corner_table = mesh_data_.corner_table();
    // Going to compute the predicted normal from the surrounding triangles
    // according to the connectivity of the given corner table.
    VertexCornersIterator<CornerTable> cit(corner_table, corner_id);
    // Position of central vertex does not change in loop.
    const VectorD<int64_t, 3> pos_cent = GetPositionForCorner(corner_id);
    // Computing normals for triangles and adding them up.

    VectorD<int64_t, 3> normal;
    while (!cit.End()) {
      // Getting corners.
      const CornerIndex c_next = corner_table->Next(corner_id);
      const CornerIndex c_prev = corner_table->Previous(corner_id);
      // Getting positions for next and previous.
      const auto pos_next = GetPositionForCorner(c_next);
      const auto pos_prev = GetPositionForCorner(c_prev);

      // Computing delta vectors to next and prev.
      const auto delta_next = pos_next - pos_cent;
      const auto delta_prev = pos_prev - pos_cent;

      // TODO(hemmer): Improve weight function.

      // Computing cross product.
      const auto cross = CrossProduct(delta_next, delta_prev);
      normal = normal + cross;
      cit.Next();
    }

    // Convert to int32_t, make sure entries are not too large.
    const int32_t abs_sum = normal.AbsSum();
    constexpr int64_t upper_bound = 1 << 29;
    if (abs_sum > upper_bound) {
      const int64_t quotient = abs_sum / upper_bound;
      normal = normal / quotient;
    }
    DCHECK_LE(normal.AbsSum(), upper_bound);
    prediction[0] = static_cast<int32_t>(normal[0]);
    prediction[1] = static_cast<int32_t>(normal[1]);
    prediction[2] = static_cast<int32_t>(normal[2]);
  }

 private:
  const PointAttribute *pos_attribute_;
  const PointIndex *entry_to_point_id_map_;
  MeshDataT mesh_data_;
};

}  // namespace draco

#endif  // DRACO_COMPRESSION_ATTRIBUTES_PREDICTION_SCHEMES_MESH_PREDICTION_SCHEME_GEOMETRIC_NORMAL_PREDICTOR_H_
