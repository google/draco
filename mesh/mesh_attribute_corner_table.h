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
#ifndef DRACO_MESH_MESH_ATTRIBUTE_CORNER_TABLE_H_
#define DRACO_MESH_MESH_ATTRIBUTE_CORNER_TABLE_H_

#include "mesh/corner_table.h"
#include "mesh/mesh.h"

namespace draco {

// Class for storing connectivity of mesh attributes. The connectivity is stored
// as a difference from the base mesh's corner table, where the differences are
// represented by attribute seam edges. This class provides a basic functionality
// for detecting the seam edges for a given attribute and for traversing the
// constrained corner table with the seam edges.
class MeshAttributeCornerTable {
 public:
  MeshAttributeCornerTable();
  bool InitEmpty(const CornerTable *table);
  bool InitFromAttribute(const Mesh *mesh, const CornerTable *table,
                         const PointAttribute *att);

  void AddSeamEdge(CornerIndex opp_corner);

  // Recomputes vertices using the newly added seam edges (needs to be called
  // whenever the seam edges are updated).
  // |mesh| and |att| can be null, in which case mapping between vertices and
  // attribute value ids is set to identity.
  void RecomputeVertices(const Mesh *mesh, const PointAttribute *att);

  inline bool IsCornerOppositeToSeamEdge(CornerIndex corner) const {
    return is_edge_on_seam_[corner.value()];
  }

  inline CornerIndex Opposite(CornerIndex corner) const {
    if (IsCornerOppositeToSeamEdge(corner))
      return kInvalidCornerIndex;
    return corner_table_->Opposite(corner);
  }

  inline CornerIndex Next(CornerIndex corner) const {
    return corner_table_->Next(corner);
  }

  inline CornerIndex Previous(CornerIndex corner) const {
    return corner_table_->Previous(corner);
  }

  // Returns true when a corner is attached to any attribute seam.
  inline bool IsCornerOnSeam(CornerIndex corner) const {
    return is_vertex_on_seam_[corner_table_->Vertex(corner).value()];
  }

  // Similar to CornerTable::GetLeftCorner and CornerTable::GetRightCorner, but
  // does not go over seam edges.
  inline CornerIndex GetLeftCorner(CornerIndex corner) const {
    return Opposite(Previous(corner));
  }
  inline CornerIndex GetRightCorner(CornerIndex corner) const {
    return Opposite(Next(corner));
  }

  // Similar to CornerTable::SwingRight, but it does not go over seam edges.
  inline CornerIndex SwingRight(CornerIndex corner) const {
    return Previous(Opposite(Previous(corner)));
  }

  // Similar to CornerTable::SwingLeft, but it does not go over seam edges.
  inline CornerIndex SwingLeft(CornerIndex corner) const {
    return Next(Opposite(Next(corner)));
  }

  int num_vertices() const { return vertex_to_attribute_entry_id_map_.size(); }
  int num_faces() const { return corner_table_->num_faces(); }

  VertexIndex Vertex(CornerIndex corner) const {
    return corner_to_vertex_map_[corner.value()];
  }

  // Returns the attribute entry id associated to the given vertex.
  VertexIndex VertexParent(VertexIndex vert) const {
    return VertexIndex(vertex_to_attribute_entry_id_map_[vert.value()].value());
  }

  inline CornerIndex LeftMostCorner(VertexIndex v) const {
    return vertex_to_left_most_corner_map_[v.value()];
  }

  inline bool IsOnBoundary(VertexIndex vert) const {
    const CornerIndex corner = LeftMostCorner(vert);
    if (corner < 0)
      return true;
    return IsCornerOnSeam(corner);
  }

  bool no_interior_seams() const { return no_interior_seams_; }
  const CornerTable *corner_table() const { return corner_table_; }

 private:
  template <bool init_vertex_to_attribute_entry_map>
  void RecomputeVerticesInternal(const Mesh *mesh, const PointAttribute *att);

  std::vector<bool> is_edge_on_seam_;
  std::vector<bool> is_vertex_on_seam_;

  // If this is set to true, it means that there are no attribute seams between
  // two faces. This can be used to speed up some algorithms.
  bool no_interior_seams_;

  std::vector<VertexIndex> corner_to_vertex_map_;

  // Map between vertices and their associated left most corners. A left most
  // corner is a corner that is adjacent to a boundary or an attribute seam from
  // right (i.e., SwingLeft from that corner will return an invalid corner). If
  // no such corner exists for a given vertex, then any corner attached to the
  // vertex can be used.
  std::vector<CornerIndex> vertex_to_left_most_corner_map_;

  // Map between vertex ids and attribute entry ids (i.e. the values stored in
  // the attribute buffer). The attribute entry id can be retrieved using the
  // VertexParent() method.
  std::vector<AttributeValueIndex> vertex_to_attribute_entry_id_map_;
  const CornerTable *corner_table_;
};

}  // namespace draco

#endif  // DRACO_MESH_MESH_ATTRIBUTE_CORNER_TABLE_H_
