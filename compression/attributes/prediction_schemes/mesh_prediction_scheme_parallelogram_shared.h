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
// Shared funcionality for different parallelogram prediction schemes.

#ifndef DRACO_COMPRESSION_ATTRIBUTES_PREDICTION_SCHEMES_MESH_PREDICTION_SCHEME_PARALLELOGRAM_SHARED_H_
#define DRACO_COMPRESSION_ATTRIBUTES_PREDICTION_SCHEMES_MESH_PREDICTION_SCHEME_PARALLELOGRAM_SHARED_H_

#include "mesh/corner_table.h"
#include "mesh/mesh.h"

namespace draco {

template <class CornerTableT>
inline void GetParallelogramEntries(
    const CornerIndex ci, const CornerTableT *table,
    const std::vector<int32_t> &vertex_to_data_map, int *opp_entry,
    int *next_entry, int *prev_entry) {
  // One vertex of the input |table| correspond to exactly one attribute value
  // entry. The |table| can be either CornerTable for per-vertex attributes,
  // or MeshAttributeCornerTable for attributes with interior seams.
  *opp_entry = vertex_to_data_map[table->Vertex(ci).value()];
  *next_entry = vertex_to_data_map[table->Vertex(table->Next(ci)).value()];
  *prev_entry = vertex_to_data_map[table->Vertex(table->Previous(ci)).value()];
}

}  // namespace draco

#endif  // DRACO_COMPRESSION_ATTRIBUTES_PREDICTION_SCHEMES_MESH_PREDICTION_SCHEME_PARALLELOGRAM_SHARED_H_
