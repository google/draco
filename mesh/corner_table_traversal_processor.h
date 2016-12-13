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
#ifndef DRACO_MESH_CORNER_TABLE_TRAVERSAL_PROCESSOR_H_
#define DRACO_MESH_CORNER_TABLE_TRAVERSAL_PROCESSOR_H_

#include "mesh/corner_table.h"

namespace draco {

// Class providing the basic traversal funcionality needed by traversers (such
// as the EdgeBreakerTraverser, see edgebreaker_traverser.h). It is used to
// return the corner table that is used for the traversal, plus it provides a
// basic book-keeping of visited faces and vertices during the traversal.
template <class CornerTableT>
class CornerTableTraversalProcessor {
 public:
  typedef CornerTableT CornerTable;

  CornerTableTraversalProcessor() : corner_table_(nullptr) {}
  virtual ~CornerTableTraversalProcessor() = default;

  void ResetProcessor(const CornerTable *corner_table) {
    corner_table_ = corner_table;
    is_face_visited_.assign(corner_table->num_faces(), false);
    ResetVertexData();
  }

  const CornerTable &GetCornerTable() const { return *corner_table_; }

  inline bool IsFaceVisited(FaceIndex face_id) const {
    if (face_id < 0)
      return true;  // Invalid faces are always considered as visited.
    return is_face_visited_[face_id.value()];
  }
  inline void MarkFaceVisited(FaceIndex face_id) {
    is_face_visited_[face_id.value()] = true;
  }
  inline bool IsVertexVisited(VertexIndex vert_id) const {
    return is_vertex_visited_[vert_id.value()];
  }
  inline void MarkVertexVisited(VertexIndex vert_id) {
    is_vertex_visited_[vert_id.value()] = true;
  }

 protected:
  virtual void ResetVertexData() {
    InitVertexData(corner_table_->num_vertices());
  }

  void InitVertexData(VertexIndex::ValueType num_verts) {
    is_vertex_visited_.assign(num_verts, false);
  }

  inline const CornerTable *corner_table() const { return corner_table_; }

 private:
  const CornerTable *corner_table_;
  std::vector<bool> is_face_visited_;
  std::vector<bool> is_vertex_visited_;
};

}  // namespace draco

#endif  // DRACO_MESH_CORNER_TABLE_TRAVERSAL_PROCESSOR_H_
