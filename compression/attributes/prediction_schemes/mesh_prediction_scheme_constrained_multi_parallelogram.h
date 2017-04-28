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
#ifndef DRACO_COMPRESSION_ATTRIBUTES_PREDICTION_SCHEMES_MESH_PREDICTION_SCHEME_CONSTRAINED_MULTI_PARALLELOGRAM_H_
#define DRACO_COMPRESSION_ATTRIBUTES_PREDICTION_SCHEMES_MESH_PREDICTION_SCHEME_CONSTRAINED_MULTI_PARALLELOGRAM_H_

#include <algorithm>
#include <cmath>

#include "compression/attributes/prediction_schemes/mesh_prediction_scheme.h"
#include "compression/attributes/prediction_schemes/mesh_prediction_scheme_parallelogram_shared.h"
#include "core/rans_bit_decoder.h"
#include "core/rans_bit_encoder.h"
#include "core/varint_decoding.h"
#include "core/varint_encoding.h"

namespace draco {

// Compared to standard multi parallelogram, constrained multi parallelogram can
// explicitly select which of the available parallelograms are going to be used
// for the prediction by marking crease edges between two triangles. This
// requires storing extra data, but it allows the predictor to avoid using
// parallelograms that would lead to poor predictions. For improved efficiency,
// our current implementation limits the maximum number of used parallelograms
// to four, which covers >95% of the cases (on average, there are only two
// parallelograms available for any given vertex).
// TODO(ostava): Split this into two classes (encoder x decoder).
template <typename DataTypeT, class TransformT, class MeshDataT>
class MeshPredictionSchemeConstrainedMultiParallelogram
    : public MeshPredictionScheme<DataTypeT, TransformT, MeshDataT> {
 public:
  using CorrType = typename PredictionScheme<DataTypeT, TransformT>::CorrType;
  using CornerTable = typename MeshDataT::CornerTable;

  explicit MeshPredictionSchemeConstrainedMultiParallelogram(
      const PointAttribute *attribute)
      : MeshPredictionScheme<DataTypeT, TransformT, MeshDataT>(attribute),
        selected_mode_(OPTIMAL_MULTI_PARALLELOGRAM) {}
  MeshPredictionSchemeConstrainedMultiParallelogram(
      const PointAttribute *attribute, const TransformT &transform,
      const MeshDataT &mesh_data)
      : MeshPredictionScheme<DataTypeT, TransformT, MeshDataT>(
            attribute, transform, mesh_data),
        selected_mode_(OPTIMAL_MULTI_PARALLELOGRAM) {}

  bool Encode(const DataTypeT *in_data, CorrType *out_corr, int size,
              int num_components,
              const PointIndex *entry_to_point_id_map) override;
  bool Decode(const CorrType *in_corr, DataTypeT *out_data, int size,
              int num_components,
              const PointIndex *entry_to_point_id_map) override;

  bool EncodePredictionData(EncoderBuffer *buffer) override;
  bool DecodePredictionData(DecoderBuffer *buffer) override;

  PredictionSchemeMethod GetPredictionMethod() const override {
    return MESH_PREDICTION_CONSTRAINED_MULTI_PARALLELOGRAM;
  }

  bool IsInitialized() const override {
    return this->mesh_data().IsInitialized();
  }

 private:
  enum Mode {
    // Selects the optimal multi-parallelogram from up to 4 available
    // parallelograms.
    OPTIMAL_MULTI_PARALLELOGRAM = 0,
  };

  static constexpr int kMaxNumParallelograms = 4;
  // Crease edges are used to store whether any given edge should be used for
  // parallelogram prediction or not. New values are added in the order in which
  // the edges are processed. For better compression, the flags are stored in
  // in separate contexts based on the number of available parallelograms at a
  // given vertex.
  std::vector<bool> is_crease_edge_[kMaxNumParallelograms];
  Mode selected_mode_;
};

template <typename DataTypeT, class TransformT, class MeshDataT>
bool MeshPredictionSchemeConstrainedMultiParallelogram<
    DataTypeT, TransformT,
    MeshDataT>::Encode(const DataTypeT *in_data, CorrType *out_corr, int size,
                       int num_components,
                       const PointIndex * /* entry_to_point_id_map */) {
  this->transform().InitializeEncoding(in_data, size, num_components);
  const CornerTable *const table = this->mesh_data().corner_table();
  const std::vector<int32_t> *const vertex_to_data_map =
      this->mesh_data().vertex_to_data_map();

  // Predicted values for all simple parallelograms encountered at any given
  // vertex.
  std::vector<DataTypeT> pred_vals[kMaxNumParallelograms];
  for (int i = 0; i < kMaxNumParallelograms; ++i) {
    pred_vals[i].resize(num_components);
  }
  // Used to store predicted value for various multi-parallelogram predictions
  // (combinations of simple parallelogram predictions).
  std::vector<DataTypeT> multi_pred_vals(num_components);

  // Struct for holding data about prediction configuration for different sets
  // of used parallelograms.
  struct PredictionConfiguration {
    PredictionConfiguration()
        : error(std::numeric_limits<int>::max()),
          configuration(0),
          num_used_parallelograms(0) {}
    int error;
    uint8_t configuration;  // Bitfield, 1 use parallelogram, 0 don't use it.
    int num_used_parallelograms;
    std::vector<DataTypeT> predicted_value;
  };

  // Bit-field used for computing permutations of excluded edges
  // (parallelograms).
  bool exluded_parallelograms[kMaxNumParallelograms];

  // We start processing the vertices from the end because this prediction uses
  // data from previous entries that could be overwritten when an entry is
  // processed.
  for (int p = this->mesh_data().data_to_corner_map()->size() - 1; p > 0; --p) {
    const CornerIndex start_corner_id =
        this->mesh_data().data_to_corner_map()->at(p);

    // Go over all corners attached to the vertex and compute the predicted
    // value from the parallelograms defined by their opposite faces.
    CornerIndex corner_id(start_corner_id);
    int num_parallelograms = 0;
    bool first_pass = true;
    while (corner_id >= 0) {
      if (ComputeParallelogramPrediction(
              p, corner_id, table, *vertex_to_data_map, in_data, num_components,
              &(pred_vals[num_parallelograms][0]))) {
        // Parallelogram prediction applied and stored in
        // |pred_vals[num_parallelograms]|
        ++num_parallelograms;
        // Stop processing when we reach the maximum number of allowed
        // parallelograms.
        if (num_parallelograms == kMaxNumParallelograms)
          break;
      }

      // Proceed to the next corner attached to the vertex. First swing left
      // and if we reach a boundary, swing right from the start corner.
      if (first_pass) {
        corner_id = table->SwingLeft(corner_id);
      } else {
        corner_id = table->SwingRight(corner_id);
      }
      if (corner_id == start_corner_id) {
        break;
      }
      if (corner_id < 0 && first_pass) {
        first_pass = false;
        corner_id = table->SwingRight(start_corner_id);
      }
    }

    // Offset to the target (destination) vertex.
    const int dst_offset = p * num_components;
    int error = 0;

    // Compute all prediction errors for all possible configurations of
    // available parallelograms.

    // Variable for holding the best configuration that has been found so far.
    PredictionConfiguration best_prediction;

    // Compute delta coding error (configuration when no parallelogram is
    // selected).
    const int src_offset = (p - 1) * num_components;
    for (int i = 0; i < num_components; ++i) {
      error += (std::abs(in_data[dst_offset + i] - in_data[src_offset + i]));
    }

    best_prediction.error = error;
    best_prediction.configuration = 0;
    best_prediction.num_used_parallelograms = 0;
    best_prediction.predicted_value.assign(
        in_data + src_offset, in_data + src_offset + num_components);

    // Compute prediction error for different cases of used parallelograms.
    for (int num_used_parallelograms = 1;
         num_used_parallelograms <= num_parallelograms;
         ++num_used_parallelograms) {
      // Mark all parallelograms as excluded.
      std::fill(exluded_parallelograms,
                exluded_parallelograms + num_parallelograms, true);
      // Mark the first |num_used_parallelograms| as not excluded.
      for (int j = 0; j < num_used_parallelograms; ++j) {
        exluded_parallelograms[j] = false;
      }
      // Permute over the excluded edges and compute error for each
      // configuration (permutation of excluded parallelograms).
      do {
        // Reset the multi-parallelogram predicted values.
        for (int j = 0; j < num_components; ++j) {
          multi_pred_vals[j] = 0;
        }
        uint8_t configuration = 0;
        for (int j = 0; j < num_parallelograms; ++j) {
          if (exluded_parallelograms[j])
            continue;
          for (int c = 0; c < num_components; ++c) {
            multi_pred_vals[c] += pred_vals[j][c];
          }
          // Set j'th bit of the configuration.
          configuration |= (1 << j);
        }
        error = 0;
        for (int j = 0; j < num_components; ++j) {
          multi_pred_vals[j] /= num_used_parallelograms;
          error += std::abs(multi_pred_vals[j] - in_data[dst_offset + j]);
        }
        if (error < best_prediction.error) {
          best_prediction.error = error;
          best_prediction.configuration = configuration;
          best_prediction.num_used_parallelograms = num_used_parallelograms;
          best_prediction.predicted_value.assign(multi_pred_vals.begin(),
                                                 multi_pred_vals.end());
        }
      } while (std::next_permutation(
          exluded_parallelograms, exluded_parallelograms + num_parallelograms));
    }

    for (int i = 0; i < num_parallelograms; ++i) {
      if ((best_prediction.configuration & (1 << i)) == 0) {
        // Parallelogram not used, mark the edge as crease.
        is_crease_edge_[num_parallelograms - 1].push_back(true);
      } else {
        // Parallelogram used. Add it to the predicted value and mark the
        // edge as not a crease.
        is_crease_edge_[num_parallelograms - 1].push_back(false);
      }
    }
    this->transform().ComputeCorrection(in_data + dst_offset,
                                        best_prediction.predicted_value.data(),
                                        out_corr, dst_offset);
  }
  // First element is always fixed because it cannot be predicted.
  for (int i = 0; i < num_components; ++i) {
    pred_vals[0][i] = static_cast<DataTypeT>(0);
  }
  this->transform().ComputeCorrection(in_data, pred_vals[0].data(), out_corr,
                                      0);
  return true;
}

template <typename DataTypeT, class TransformT, class MeshDataT>
bool MeshPredictionSchemeConstrainedMultiParallelogram<
    DataTypeT, TransformT,
    MeshDataT>::Decode(const CorrType *in_corr, DataTypeT *out_data,
                       int /* size */, int num_components,
                       const PointIndex * /* entry_to_point_id_map */) {
  this->transform().InitializeDecoding(num_components);

  // Predicted values for all simple parallelograms encountered at any given
  // vertex.
  std::vector<DataTypeT> pred_vals[kMaxNumParallelograms];
  for (int i = 0; i < kMaxNumParallelograms; ++i) {
    pred_vals[i].resize(num_components, 0);
  }
  this->transform().ComputeOriginalValue(pred_vals[0].data(), in_corr, out_data,
                                         0);

  const CornerTable *const table = this->mesh_data().corner_table();
  const std::vector<int32_t> *const vertex_to_data_map =
      this->mesh_data().vertex_to_data_map();

  // Current position in the |is_crease_edge_| array for each context.
  std::vector<int> is_crease_edge_pos(kMaxNumParallelograms, 0);

  // Used to store predicted value for multi-parallelogram prediction.
  std::vector<DataTypeT> multi_pred_vals(num_components);

  const int corner_map_size = this->mesh_data().data_to_corner_map()->size();
  for (int p = 1; p < corner_map_size; ++p) {
    const CornerIndex start_corner_id =
        this->mesh_data().data_to_corner_map()->at(p);

    CornerIndex corner_id(start_corner_id);
    int num_parallelograms = 0;
    bool first_pass = true;
    while (corner_id >= 0) {
      if (ComputeParallelogramPrediction(
              p, corner_id, table, *vertex_to_data_map, out_data,
              num_components, &(pred_vals[num_parallelograms][0]))) {
        // Parallelogram prediction applied and stored in
        // |pred_vals[num_parallelograms]|
        ++num_parallelograms;
        // Stop processing when we reach the maximum number of allowed
        // parallelograms.
        if (num_parallelograms == kMaxNumParallelograms)
          break;
      }

      // Proceed to the next corner attached to the vertex. First swing left
      // and if we reach a boundary, swing right from the start corner.
      if (first_pass) {
        corner_id = table->SwingLeft(corner_id);
      } else {
        corner_id = table->SwingRight(corner_id);
      }
      if (corner_id == start_corner_id) {
        break;
      }
      if (corner_id < 0 && first_pass) {
        first_pass = false;
        corner_id = table->SwingRight(start_corner_id);
      }
    }

    // Check which of the available parallelograms are actually used and compute
    // the final predicted value.
    int num_used_parallelograms = 0;
    if (num_parallelograms > 0) {
      for (int i = 0; i < num_components; ++i) {
        multi_pred_vals[i] = 0;
      }
      // Check which parallelograms are actually used.
      for (int i = 0; i < num_parallelograms; ++i) {
        const int context = num_parallelograms - 1;
        const bool is_crease =
            is_crease_edge_[context][is_crease_edge_pos[context]++];
        if (!is_crease) {
          ++num_used_parallelograms;
          for (int j = 0; j < num_components; ++j) {
            multi_pred_vals[j] += pred_vals[i][j];
          }
        }
      }
    }
    const int dst_offset = p * num_components;
    if (num_used_parallelograms == 0) {
      // No parallelogram was valid.
      // We use the last decoded point as a reference.
      const int src_offset = (p - 1) * num_components;
      this->transform().ComputeOriginalValue(out_data + src_offset, in_corr,
                                             out_data + dst_offset, dst_offset);
    } else {
      // Compute the correction from the predicted value.
      for (int c = 0; c < num_components; ++c) {
        multi_pred_vals[c] /= num_used_parallelograms;
      }
      this->transform().ComputeOriginalValue(multi_pred_vals.data(), in_corr,
                                             out_data + dst_offset, dst_offset);
    }
  }
  return true;
}

template <typename DataTypeT, class TransformT, class MeshDataT>
bool MeshPredictionSchemeConstrainedMultiParallelogram<
    DataTypeT, TransformT, MeshDataT>::EncodePredictionData(EncoderBuffer
                                                                *buffer) {
  // Encode prediction mode.
  buffer->Encode(static_cast<uint8_t>(selected_mode_));

  // Encode selected edges using separate rans bit coder for each context.
  for (int i = 0; i < kMaxNumParallelograms; ++i) {
    // |i| is the context based on the number of available parallelograms, which
    // is always equal to |i + 1|.
    const int num_used_parallelograms = i + 1;
    EncodeVarint<uint32_t>(is_crease_edge_[i].size(), buffer);
    if (is_crease_edge_[i].size()) {
      RAnsBitEncoder encoder;
      encoder.StartEncoding();
      // Encode the crease edge flags in the reverse vertex order that is needed
      // be the decoder. Note that for the currently supported mode, each vertex
      // has exactly |num_used_parallelograms| edges that need to be encoded.
      for (int j = is_crease_edge_[i].size() - num_used_parallelograms; j >= 0;
           j -= num_used_parallelograms) {
        // Go over all edges of the current vertex.
        for (int k = 0; k < num_used_parallelograms; ++k) {
          encoder.EncodeBit(is_crease_edge_[i][j + k]);
        }
      }
      encoder.EndEncoding(buffer);
    }
  }
  return MeshPredictionScheme<DataTypeT, TransformT,
                              MeshDataT>::EncodePredictionData(buffer);
}

template <typename DataTypeT, class TransformT, class MeshDataT>
bool MeshPredictionSchemeConstrainedMultiParallelogram<
    DataTypeT, TransformT, MeshDataT>::DecodePredictionData(DecoderBuffer
                                                                *buffer) {
  // Decode prediction mode.
  uint8_t mode;
  if (!buffer->Decode(&mode)) {
    return false;
  }

  if (mode != OPTIMAL_MULTI_PARALLELOGRAM) {
    // Unsupported mode.
    return false;
  }

  // Encode selected edges using separate rans bit coder for each context.
  for (int i = 0; i < kMaxNumParallelograms; ++i) {
    uint32_t num_flags;
    DecodeVarint<uint32_t>(&num_flags, buffer);
    if (num_flags > 0) {
      is_crease_edge_[i].resize(num_flags);
      RAnsBitDecoder decoder;
      decoder.StartDecoding(buffer);
      for (int j = 0; j < num_flags; ++j) {
        is_crease_edge_[i][j] = decoder.DecodeNextBit();
      }
      decoder.EndDecoding();
    }
  }
  return MeshPredictionScheme<DataTypeT, TransformT,
                              MeshDataT>::DecodePredictionData(buffer);
}

}  // namespace draco

#endif  // DRACO_COMPRESSION_ATTRIBUTES_PREDICTION_SCHEMES_MESH_PREDICTION_SCHEME_CONSTRAINED_MULTI_PARALLELOGRAM_H_
