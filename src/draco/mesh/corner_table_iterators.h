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
#ifndef DRACO_MESH_CORNER_TABLE_ITERATORS_H_
#define DRACO_MESH_CORNER_TABLE_ITERATORS_H_

#include "draco/mesh/corner_table.h"

namespace draco {

// TODO(ostava): Move the remaining corner table iterators here.

// Class for iterating over corners attached to a specified vertex.
template <class CornerTableT = CornerTable>
class VertexCornersIterator
    : public std::iterator<std::forward_iterator_tag, CornerIndex> {
 public:
  // std::iterator interface requires a default constructor.
  VertexCornersIterator()
      : corner_table_(nullptr),
        start_corner_(-1),
        corner_(start_corner_),
        left_traversal_(true) {}

  // Create the iterator from the provided corner table and the central vertex.
  VertexCornersIterator(const CornerTableT *table, VertexIndex vert_id)
      : corner_table_(table),
        start_corner_(table->LeftMostCorner(vert_id)),
        corner_(start_corner_),
        left_traversal_(true) {}

  // Create the iterator from the provided corner table and the first corner.
  VertexCornersIterator(const CornerTableT *table, CornerIndex corner_id)
      : corner_table_(table),
        start_corner_(corner_id),
        corner_(start_corner_),
        left_traversal_(true) {}

  // Gets the last visited corner.
  CornerIndex Corner() const { return corner_; }

  // Returns true when all ring vertices have been visited.
  bool End() const { return corner_ == kInvalidCornerIndex; }

  // Proceeds to the next corner if possible.
  void Next() {
    if (left_traversal_) {
      corner_ = corner_table_->SwingLeft(corner_);
      if (corner_ == kInvalidCornerIndex) {
        // Open boundary reached.
        corner_ = corner_table_->SwingRight(start_corner_);
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
  CornerIndex operator*() const { return Corner(); }
  VertexCornersIterator &operator++() {
    Next();
    return *this;
  }
  VertexCornersIterator operator++(int) {
    const VertexCornersIterator result = *this;
    ++(*this);
    return result;
  }
  bool operator!=(const VertexCornersIterator &other) const {
    return corner_ != other.corner_ || start_corner_ != other.start_corner_;
  }
  bool operator==(const VertexCornersIterator &other) const {
    return !this->operator!=(other);
  }

  // Helper function for getting a valid end iterator.
  static VertexCornersIterator EndIterator(VertexCornersIterator other) {
    VertexCornersIterator ret = other;
    ret.corner_ = kInvalidCornerIndex;
    return ret;
  }

 protected:
  const CornerTableT *corner_table() const { return corner_table_; }
  CornerIndex start_corner() const { return start_corner_; }
  CornerIndex &corner() { return corner_; }
  bool is_left_traversal() const { return left_traversal_; }
  void swap_traversal() { left_traversal_ = !left_traversal_; }

 private:
  const CornerTableT *corner_table_;
  // The first processed corner.
  CornerIndex start_corner_;
  // The last processed corner.
  CornerIndex corner_;
  // Traversal direction.
  bool left_traversal_;
};

}  // namespace draco

#endif  // DRACO_MESH_CORNER_TABLE_ITERATORS_H_
