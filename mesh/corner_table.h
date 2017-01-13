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
#ifndef DRACO_MESH_CORNER_TABLE_H_
#define DRACO_MESH_CORNER_TABLE_H_

#include <memory>

#include "core/draco_index_type_vector.h"
#include "mesh/corner_table_indices.h"

namespace draco {

// CornerTable is used to represent connectivity of triangular meshes.
// For every corner of all faces, the corner table stores the index of the
// opposite corner in the neighboring face (if it exists) as illustrated in the
// figure below (see corner |c| and it's opposite corner |o|).
//
//     *
//    /c\
//   /   \
//  /n   p\
// *-------*
//  \     /
//   \   /
//    \o/
//     *
//
// All corners are defined by unique CornerIndex and each triplet of corners
// that define a single face id always ordered consecutively as:
//     { 3 * FaceIndex, 3 * FaceIndex + 1, 3 * FaceIndex +2 }.
// This representation of corners allows CornerTable to easily retrieve Next and
// Previous corners on any face (see corners |n| and |p| in the figure above).
// Using the Next, Previous, and Opposite corners then enables traversal of any
// 2-manifold surface.
// If the CornerTable is constructed from a non-manifold surface, the input
// non-manifold edges and vertices are automatically split.
class CornerTable {
 public:
  CornerTable();
  static std::unique_ptr<CornerTable> Create(
      const IndexTypeVector<FaceIndex, FaceType> &faces);

  // Initializes the CornerTable from provides set of indexed faces.
  // The input faces can represent a non-manifold topolgy, in which case the
  // non-manifold edges and vertices are going to be split.
  bool Initialize(const IndexTypeVector<FaceIndex, FaceType> &faces);

  // Resets the corner table to the given number of invalid faces.
  void Reset(int num_faces);

  inline int num_vertices() const { return vertex_corners_.size(); }
  inline int num_corners() const { return faces_.size() * 3; }
  inline int num_faces() const { return faces_.size(); }

  inline CornerIndex Opposite(CornerIndex corner) const {
    if (corner < 0)
      return corner;
    return opposite_corners_[corner];
  }
  inline CornerIndex Next(CornerIndex corner) const {
    if (corner < 0)
      return corner;
    return LocalIndex(++corner) ? corner : corner - 3;
  }
  inline CornerIndex Previous(CornerIndex corner) const {
    if (corner < 0)
      return corner;
    return LocalIndex(corner) ? corner - 1 : corner + 2;
  }
  inline VertexIndex Vertex(CornerIndex corner) const {
    if (corner < 0)
      return kInvalidVertexIndex;
    return faces_[Face(corner)][LocalIndex(corner)];
  }
  inline FaceIndex Face(CornerIndex corner) const {
    if (corner < 0)
      return kInvalidFaceIndex;
    return FaceIndex(corner.value() / 3);
  }
  inline CornerIndex FirstCorner(FaceIndex face) const {
    if (face < 0)
      return kInvalidCornerIndex;
    return CornerIndex(face.value() * 3);
  }
  inline std::array<CornerIndex, 3> AllCorners(FaceIndex face) const {
    const CornerIndex ci = CornerIndex(face.value() * 3);
    return {{ci, ci + 1, ci + 2}};
  }
  inline int LocalIndex(CornerIndex corner) const { return corner.value() % 3; }
  inline FaceType FaceData(FaceIndex face) const { return faces_[face]; }

  void SetFaceData(FaceIndex face, FaceType data) { faces_[face] = data; }

  // Returns the left-most corner of a single vertex 1-ring. If a vertex is not
  // on a boundary (in which case it has a full 1-ring), this function returns
  // any of the corners mapped to the given vertex.
  inline CornerIndex LeftMostCorner(VertexIndex v) const {
    return vertex_corners_[v];
  }

  // Returns the parent vertex index of a given corner table vertex.
  VertexIndex VertexParent(VertexIndex vertex) const {
    if (vertex.value() < num_original_vertices_)
      return vertex;
    return non_manifold_vertex_parents_[vertex - num_original_vertices_];
  }

  // Returns true if the corner is valid.
  inline bool IsValid(CornerIndex c) const {
    return Vertex(c) != kInvalidVertexIndex;
  }

  // Returns the valence (or degree) of a vertex.
  // Returns -1 if the given vertex index is not valid.
  int Valence(VertexIndex v) const;

  // Returns the valence of the vertex at the given corner.
  inline int Valence(CornerIndex c) const {
    if (c == kInvalidCornerIndex)
      return -1;
    return Valence(Vertex(c));
  }

  // Returns true if the specified vertex is on a boundary.
  inline bool IsOnBoundary(VertexIndex vert) const {
    const CornerIndex corner = LeftMostCorner(vert);
    if (SwingLeft(corner) < 0)
      return true;
    return false;
  }

  //     *-------*
  //    / \     / \
  //   /   \   /   \
  //  /   sl\c/sr   \
  // *-------v-------*
  // Returns the corner on the adjacent face on the right that maps to
  // the same vertex as the given corner (sr in the above diagram).
  inline CornerIndex SwingRight(CornerIndex corner) const {
    return Previous(Opposite(Previous(corner)));
  }
  // Returns the corner on the left face that maps to the same vertex as the
  // given corner (sl in the above diagram).
  inline CornerIndex SwingLeft(CornerIndex corner) const {
    return Next(Opposite(Next(corner)));
  }

  // Get opposite corners on the left and right faces respecitively (see image
  // below, where L and R are the left and right corners of a corner X.
  //
  // *-------*-------*
  //  \L    /X\    R/
  //   \   /   \   /
  //    \ /     \ /
  //     *-------*
  CornerIndex GetLeftCorner(CornerIndex corner_id) const {
    if (corner_id < 0)
      return kInvalidCornerIndex;
    return Opposite(Previous(corner_id));
  }
  CornerIndex GetRightCorner(CornerIndex corner_id) const {
    if (corner_id < 0)
      return kInvalidCornerIndex;
    return Opposite(Next(corner_id));
  }

  // Returns the number of new vertices that were created as a result of
  // spliting of non-manifold vertices of the input geometry.
  int NumNewVertices() const { return num_vertices() - num_original_vertices_; }
  int NumOriginalVertices() const { return num_original_vertices_; }

  // Returns the number of faces with duplicated vertex indices.
  int NumDegeneratedFaces() const { return num_degenerated_faces_; }

  // Returns the number of isolated vertices (vertices that have
  // vertex_corners_ mapping set to kInvalidCornerIndex.
  int NumIsolatedVertices() const { return num_isolated_vertices_; }

  bool IsDegenerated(FaceIndex face) const;

  // Methods that modify an existing corner table.
  // Sets the opposite corner mapping between two corners. Caller must ensure
  // that the indices are valid.
  inline void SetOppositeCorner(CornerIndex corner_id,
                                CornerIndex opp_corner_id) {
    opposite_corners_[corner_id] = opp_corner_id;
  }

  // Sets opposite corners for both input corners.
  inline void SetOppositeCorners(CornerIndex corner_0, CornerIndex corner_1) {
    if (corner_0 != kInvalidCornerIndex)
      SetOppositeCorner(corner_0, corner_1);
    if (corner_1 != kInvalidCornerIndex)
      SetOppositeCorner(corner_1, corner_0);
  }

  // Updates mapping betweeh a corner and a vertex.
  inline void MapCornerToVertex(CornerIndex corner_id, VertexIndex vert_id) {
    const FaceIndex face = Face(corner_id);
    faces_[face][LocalIndex(corner_id)] = vert_id;
    if (vert_id >= 0) {
      if (vertex_corners_.size() <= static_cast<size_t>(vert_id.value()))
        vertex_corners_.resize(vert_id.value() + 1);
      vertex_corners_[vert_id] = corner_id;
    }
  }

  VertexIndex AddNewVertex() {
    // Add a new invalid vertex.
    vertex_corners_.push_back(kInvalidCornerIndex);
    return VertexIndex(vertex_corners_.size() - 1);
  }

  // Sets a new left most corner for a given vertex.
  void SetLeftMostCorner(VertexIndex vert, CornerIndex corner) {
    if (vert != kInvalidVertexIndex)
      vertex_corners_[vert] = corner;
  }

  // Updates the vertex to corner map on a specified vertex. This should be
  // called in cases where the mapping may be invalid (e.g. when the corner
  // table was constructed manually).
  void UpdateVertexToCornerMap(VertexIndex vert) {
    const CornerIndex first_c = vertex_corners_[vert];
    if (first_c < 0)
      return;  // Isolated vertex.
    CornerIndex act_c = SwingLeft(first_c);
    CornerIndex c = first_c;
    while (act_c >= 0 && act_c != first_c) {
      c = act_c;
      act_c = SwingLeft(act_c);
    }
    if (act_c != first_c) {
      vertex_corners_[vert] = c;
    }
  }

  // Sets the new number of vertices. It's a responsibility of the caller to
  // ensure that no corner is mapped beyond the range of the new number of
  // vertices.
  inline void SetNumVertices(int num_vertices) {
    vertex_corners_.resize(num_vertices, kInvalidCornerIndex);
  }

  // Makes a vertex isolated (not attached to any corner).
  void MakeVertexIsolated(VertexIndex vert) {
    vertex_corners_[vert] = kInvalidCornerIndex;
  }

  // Returns true if a vertex is not attached to any face.
  inline bool IsVertexIsolated(VertexIndex v) const {
    return LeftMostCorner(v) < 0;
  }

  // Makes a given face invalid (all corners are marked as invalid).
  void MakeFaceInvalid(FaceIndex face) {
    if (face != kInvalidFaceIndex) {
      for (int i = 0; i < 3; ++i) {
        faces_[face][i] = kInvalidVertexIndex;
      }
    }
  }

  // Updates mapping between faces and a vertex using the corners mapped to
  // the provided vertex.
  void UpdateFaceToVertexMap(const VertexIndex vertex);

 private:
  // Computes opposite corners mapping from the data stored in |faces_|. Any
  // non-manifold edge will be split so the result is always a 2-manifold
  // surface.
  bool ComputeOppositeCorners(int *num_vertices);

  // Computes the lookup map for going from a vertex to a corner. This method
  // can handle non-manifold vertices by spliting them into multiple manifold
  // vertices.
  bool ComputeVertexCorners(int num_vertices);

  IndexTypeVector<FaceIndex, FaceType> faces_;
  IndexTypeVector<CornerIndex, CornerIndex> opposite_corners_;
  IndexTypeVector<VertexIndex, CornerIndex> vertex_corners_;

  int num_original_vertices_;
  int num_degenerated_faces_;
  int num_isolated_vertices_;
  IndexTypeVector<VertexIndex, VertexIndex> non_manifold_vertex_parents_;
};

// TODO(ostava): All these iterators will be moved into a new file in a separate
// CL.

// Class for iterating over vertices in a 1-ring around the specified vertex.
class VertexRingIterator
    : public std::iterator<std::forward_iterator_tag, VertexIndex> {
 public:
  // std::iterator interface requires a default constructor.
  VertexRingIterator()
      : corner_table_(nullptr),
        start_corner_(kInvalidCornerIndex),
        corner_(start_corner_),
        left_traversal_(true) {}

  // Create the iterator from the provided corner table and the central vertex.
  VertexRingIterator(const CornerTable *table, VertexIndex vert_id)
      : corner_table_(table),
        start_corner_(table->LeftMostCorner(vert_id)),
        corner_(start_corner_),
        left_traversal_(true) {}

  // Gets the last visited ring vertex.
  VertexIndex Vertex() const {
    CornerIndex ring_corner = left_traversal_ ? corner_table_->Previous(corner_)
                                              : corner_table_->Next(corner_);
    return corner_table_->Vertex(ring_corner);
  }

  // Returns true when all ring vertices have been visited.
  bool End() const { return corner_ < 0; }

  // Proceeds to the next ring vertex if possible.
  void Next() {
    if (left_traversal_) {
      corner_ = corner_table_->SwingLeft(corner_);
      if (corner_ < 0) {
        // Open boundary reached.
        corner_ = start_corner_;
        left_traversal_ = false;
      } else if (corner_ == start_corner_) {
        // End reached.
        corner_ = kInvalidCornerIndex;
      }
    } else {
      // Go to the right until we reach a boundary there (no explicit check
      // is needed in this case).
      corner_ = corner_table_->SwingRight(corner_);
    }
  }

  // std::iterator interface.
  value_type operator*() const { return Vertex(); }
  VertexRingIterator &operator++() {
    Next();
    return *this;
  }
  VertexRingIterator operator++(int) {
    const VertexRingIterator result = *this;
    ++(*this);
    return result;
  }
  bool operator!=(const VertexRingIterator &other) const {
    return corner_ != other.corner_ || start_corner_ != other.start_corner_;
  }
  bool operator==(const VertexRingIterator &other) const {
    return !this->operator!=(other);
  }

  // Helper function for getting a valid end iterator.
  static VertexRingIterator EndIterator(VertexRingIterator other) {
    VertexRingIterator ret = other;
    ret.corner_ = kInvalidCornerIndex;
    return ret;
  }

 private:
  const CornerTable *corner_table_;
  // The first processed corner.
  CornerIndex start_corner_;
  // The last processed corner.
  CornerIndex corner_;
  // Traversal direction.
  bool left_traversal_;
};

// Class for iterating over faces adjacent to the specified input face.
class FaceAdjacencyIterator
    : public std::iterator<std::forward_iterator_tag, FaceIndex> {
 public:
  // std::iterator interface requires a default constructor.
  FaceAdjacencyIterator()
      : corner_table_(nullptr),
        start_corner_(kInvalidCornerIndex),
        corner_(start_corner_) {}

  // Create the iterator from the provided corner table and the central vertex.
  FaceAdjacencyIterator(const CornerTable *table, FaceIndex face_id)
      : corner_table_(table),
        start_corner_(table->FirstCorner(face_id)),
        corner_(start_corner_) {
    // We need to start with a corner that has a valid opposite face (if
    // there is any such corner).
    if (corner_table_->Opposite(corner_) < 0)
      FindNextFaceNeighbor();
  }

  // Gets the last visited adjacent face.
  FaceIndex Face() const {
    return corner_table_->Face(corner_table_->Opposite(corner_));
  }

  // Returns true when all adjacent faces have been visited.
  bool End() const { return corner_ < 0; }

  // Proceeds to the next adjacen face if possible.
  void Next() { FindNextFaceNeighbor(); }

  // std::iterator interface.
  value_type operator*() const { return Face(); }
  FaceAdjacencyIterator &operator++() {
    Next();
    return *this;
  }
  FaceAdjacencyIterator operator++(int) {
    const FaceAdjacencyIterator result = *this;
    ++(*this);
    return result;
  }
  bool operator!=(const FaceAdjacencyIterator &other) const {
    return corner_ != other.corner_ || start_corner_ != other.start_corner_;
  }
  bool operator==(const FaceAdjacencyIterator &other) const {
    return !this->operator!=(other);
  }

  // Helper function for getting a valid end iterator.
  static FaceAdjacencyIterator EndIterator(FaceAdjacencyIterator other) {
    FaceAdjacencyIterator ret = other;
    ret.corner_ = kInvalidCornerIndex;
    return ret;
  }

 private:
  // Finds the next corner with a valid opposite face.
  void FindNextFaceNeighbor() {
    while (corner_ >= 0) {
      corner_ = corner_table_->Next(corner_);
      if (corner_ == start_corner_) {
        corner_ = kInvalidCornerIndex;
        return;
      }
      if (corner_table_->Opposite(corner_) >= 0) {
        // Valid opposite face.
        return;
      }
    }
  }

  const CornerTable *corner_table_;
  // The first processed corner.
  CornerIndex start_corner_;
  // The last processed corner.
  CornerIndex corner_;
};

}  // namespace draco

#endif  // DRACO_MESH_CORNER_TABLE_H_
