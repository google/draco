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
#ifndef DRACO_COMPRESSION_ATTRIBUTES_PREDICTION_SCHEMES_MESH_PREDICTION_SCHEME_MULTI_PARALLELOGRAM_H_
#define DRACO_COMPRESSION_ATTRIBUTES_PREDICTION_SCHEMES_MESH_PREDICTION_SCHEME_MULTI_PARALLELOGRAM_H_

#include "compression/attributes/prediction_schemes/mesh_prediction_scheme.h"
#include "compression/attributes/prediction_schemes/mesh_prediction_scheme_parallelogram_shared.h"

namespace draco {

// Multi parallelogram prediction predicts attribute values using information
// from all opposite faces to the predicted vertex, compared to the standard
// prediction scheme, where only one opposite face is used (see
// prediction_scheme_parallelogram.h). This approach is generally slower than
// the standard parallelogram prediction, but it usually results in better
// prediction (5 - 20% based on the quantization level. Better gains can be
// achieved when more aggressive quantization is used).
// TODO(ostava): Rename. The new name should reflect the fact that we need mesh
// data.
template <typename DataTypeT, class TransformT, class MeshDataT>
class MeshPredictionSchemeMultiParallelogram
    : public MeshPredictionScheme<DataTypeT, TransformT, MeshDataT> {
 public:
  using CorrType = typename PredictionScheme<DataTypeT, TransformT>::CorrType;
  using CornerTable = typename MeshDataT::CornerTable;

  explicit MeshPredictionSchemeMultiParallelogram(
      const PointAttribute *attribute)
      : MeshPredictionScheme<DataTypeT, TransformT, MeshDataT>(attribute) {}
  MeshPredictionSchemeMultiParallelogram(const PointAttribute *attribute,
                                         const TransformT &transform,
                                         const MeshDataT &mesh_data)
      : MeshPredictionScheme<DataTypeT, TransformT, MeshDataT>(
            attribute, transform, mesh_data) {}

  bool Encode(const DataTypeT *in_data, CorrType *out_corr, int size,
              int num_components,
              const PointIndex *entry_to_point_id_map) override;
  bool Decode(const CorrType *in_corr, DataTypeT *out_data, int size,
              int num_components,
              const PointIndex *entry_to_point_id_map) override;
  PredictionSchemeMethod GetPredictionMethod() const override {
    return MESH_PREDICTION_MULTI_PARALLELOGRAM;
  }

  bool IsInitialized() const override {
    return this->mesh_data().IsInitialized();
  }
};

template <typename DataTypeT, class TransformT, class MeshDataT>
bool MeshPredictionSchemeMultiParallelogram<DataTypeT, TransformT, MeshDataT>::
    Encode(const DataTypeT *in_data, CorrType *out_corr, int size,
           int num_components, const PointIndex * /* entry_to_point_id_map */) {
  this->transform().InitializeEncoding(in_data, size, num_components);
  const CornerTable *const table = this->mesh_data().corner_table();
  const std::vector<int32_t> *const vertex_to_data_map =
      this->mesh_data().vertex_to_data_map();

  std::unique_ptr<DataTypeT[]> pred_vals(new DataTypeT[num_components]());

  // We start processing from the end because this prediction uses data from
  // previous entries that could be overwritten when an entry is processed.
  for (int p = this->mesh_data().data_to_corner_map()->size() - 1; p > 0; --p) {
    const CornerIndex start_corner_id =
        this->mesh_data().data_to_corner_map()->at(p);

    // Go over all corners attached to the vertex and compute the predicted
    // value from the parallelograms defined by their opposite faces.
    CornerIndex corner_id(start_corner_id);
    int num_parallelograms = 0;
    for (int i = 0; i < num_components; ++i) {
      pred_vals[i] = static_cast<DataTypeT>(0);
    }
    while (corner_id >= 0) {
      // TODO(ostava): Move code shared between multiple predictors into a new
      // file.
      int vert_opp = p, vert_next = p, vert_prev = p;
      const CornerIndex opp_corner = table->Opposite(corner_id);
      if (opp_corner >= 0) {
        GetParallelogramEntries(opp_corner, table, *vertex_to_data_map,
                                &vert_opp, &vert_next, &vert_prev);
      }
      if (vert_opp < p && vert_next < p && vert_prev < p) {
        // Apply the parallelogram prediction.
        const int v_opp_off = vert_opp * num_components;
        const int v_next_off = vert_next * num_components;
        const int v_prev_off = vert_prev * num_components;
        for (int c = 0; c < num_components; ++c) {
          pred_vals[c] += (in_data[v_next_off + c] + in_data[v_prev_off + c]) -
                          in_data[v_opp_off + c];
        }
        ++num_parallelograms;
      }

      // Proceed to the next corner attached to the vertex.
      // TODO(ostava): This will not go around the whole neighborhood on
      // vertices on a mesh boundary. We need to SwingLeft from the start vertex
      // again to get the full coverage.
      corner_id = table->SwingRight(corner_id);
      if (corner_id == start_corner_id) {
        corner_id = kInvalidCornerIndex;
      }
    }
    const int dst_offset = p * num_components;
    if (num_parallelograms == 0) {
      // No parallelogram was valid.
      // We use the last encoded point as a reference.
      const int src_offset = (p - 1) * num_components;
      this->transform().ComputeCorrection(
          in_data + dst_offset, in_data + src_offset, out_corr, dst_offset);
    } else {
      // Compute the correction from the predicted value.
      for (int c = 0; c < num_components; ++c) {
        pred_vals[c] /= num_parallelograms;
      }
      this->transform().ComputeCorrection(in_data + dst_offset, pred_vals.get(),
                                          out_corr, dst_offset);
    }
  }
  // First element is always fixed because it cannot be predicted.
  for (int i = 0; i < num_components; ++i) {
    pred_vals[i] = static_cast<DataTypeT>(0);
  }
  this->transform().ComputeCorrection(in_data, pred_vals.get(), out_corr, 0);
  return true;
}

template <typename DataTypeT, class TransformT, class MeshDataT>
bool MeshPredictionSchemeMultiParallelogram<DataTypeT, TransformT, MeshDataT>::
    Decode(const CorrType *in_corr, DataTypeT *out_data, int /* size */,
           int num_components, const PointIndex * /* entry_to_point_id_map */) {
  this->transform().InitializeDecoding(num_components);

  std::unique_ptr<DataTypeT[]> pred_vals(new DataTypeT[num_components]());

  this->transform().ComputeOriginalValue(pred_vals.get(), in_corr, out_data, 0);

  const CornerTable *const table = this->mesh_data().corner_table();
  const std::vector<int32_t> *const vertex_to_data_map =
      this->mesh_data().vertex_to_data_map();

  const int corner_map_size = this->mesh_data().data_to_corner_map()->size();
  for (int p = 1; p < corner_map_size; ++p) {
    const CornerIndex start_corner_id =
        this->mesh_data().data_to_corner_map()->at(p);

    CornerIndex corner_id(start_corner_id);
    int num_parallelograms = 0;
    for (int i = 0; i < num_components; ++i) {
      pred_vals[i] = static_cast<DataTypeT>(0);
    }
    while (corner_id >= 0) {
      int vert_opp = p, vert_next = p, vert_prev = p;
      const CornerIndex opp_corner = table->Opposite(corner_id);
      if (opp_corner >= 0) {
        GetParallelogramEntries(opp_corner, table, *vertex_to_data_map,
                                &vert_opp, &vert_next, &vert_prev);
      }
      if (vert_opp < p && vert_next < p && vert_prev < p) {
        // Apply the parallelogram prediction.
        const int v_opp_off = vert_opp * num_components;
        const int v_next_off = vert_next * num_components;
        const int v_prev_off = vert_prev * num_components;
        for (int c = 0; c < num_components; ++c) {
          pred_vals[c] +=
              (out_data[v_next_off + c] + out_data[v_prev_off + c]) -
              out_data[v_opp_off + c];
        }
        ++num_parallelograms;
      }

      corner_id = table->SwingRight(corner_id);
      if (corner_id == start_corner_id) {
        corner_id = kInvalidCornerIndex;
      }
    }

    const int dst_offset = p * num_components;
    if (num_parallelograms == 0) {
      // No parallelogram was valid.
      // We use the last decoded point as a reference.
      const int src_offset = (p - 1) * num_components;
      this->transform().ComputeOriginalValue(out_data + src_offset, in_corr,
                                             out_data + dst_offset, dst_offset);
    } else {
      // Compute the correction from the predicted value.
      for (int c = 0; c < num_components; ++c) {
        pred_vals[c] /= num_parallelograms;
      }
      this->transform().ComputeOriginalValue(pred_vals.get(), in_corr,
                                             out_data + dst_offset, dst_offset);
    }
  }
  return true;
}

}  // namespace draco

#endif  // DRACO_COMPRESSION_ATTRIBUTES_PREDICTION_SCHEMES_MESH_PREDICTION_SCHEME_MULTI_PARALLELOGRAM_H_
