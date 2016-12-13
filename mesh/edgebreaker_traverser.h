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
#ifndef DRACO_MESH_EDGEBREAKER_TRAVERSER_H_
#define DRACO_MESH_EDGEBREAKER_TRAVERSER_H_

#include <vector>

#include "mesh/corner_table.h"
#include "mesh/edgebreaker_observer.h"

namespace draco {

// Basic framework for edgebreaker traversal over a corner table data
// structure. The traversal and the callbacks are handled through the template
// arguments TraversalProcessorT, TraversalObserverT and EdgeBreakerObserverT.
// TraversalProcessorT is used to provide infrastructure for handling of visited
// vertices and faces, TraversalObserverT can be used to implement custom
// callbacks for varous traversal events, and EdgeBreakerObserverT can be used
// to provide handling of edgebreaker symbols.
// TraversalProcessorT needs to define the type of the corner table as:
//
// TraversalProcessorT::CornerTable
//
// and it needs to implement the following methods:
//
// CornerTable GetCornerTable();
//    - Returns corner table for a given processor.
//
// bool IsVertexVisited(VertexIndex vertex);
//    - Returns true if the vertex has been already marked as visited during
//      the traversal.
//
// void MarkVertexVisited(VertexIndex vertex);
//    - Informs the processor that the vertex has been reached.
//
// bool IsFaceVisited(FaceIndex face);
//    - Returns true if the face has been already marked as visited during
//      the traversal.
//
// void MarkFaceVisited(FaceIndex face);
//    - Should be used to mark the newly visited face.
//
// ----------------------------------------------------
//
// TraversalObserverT can perform an action on a traversal event such as newly
// visited face, or corner, but it does not affect the traversal itself.
// It needs to implement the following methods:
//
// void OnNewFaceVisited(FaceIndex face);
//    - Called whenever a previously unvisited face is reached.
//
// void OnNewVertexVisited(VertexIndex vert, CornerIndex corner)
//    - Called when a new vertex is visited. |corner| is used to indicate the
//      which of the vertex's corners has been reached.
//
// ----------------------------------------------------
//
// EdgeBreakerObserverT then needs to implement following methods:
//
// void OnSymbolC();
// void OnSymbolL();
// void OnSymbolR();
// void OnSymbolE();
// void OnSymbolS();
//    - Informs the observer about the configuration of a newly visited face.

template <class TraversalProcessorT, class TraversalObserverT,
          class EdgeBreakerObserverT = EdgeBreakerObserver>
class EdgeBreakerTraverser {
 public:
  typedef typename TraversalProcessorT::CornerTable CornerTable;

  EdgeBreakerTraverser() {}
  void Init(TraversalProcessorT processor) {
    corner_table_ = &processor.GetCornerTable();
    processor_ = processor;
  }
  void Init(TraversalProcessorT processor,
            TraversalObserverT traversal_observer) {
    Init(processor);
    traversal_observer_ = traversal_observer;
  }
  void Init(TraversalProcessorT processor,
            TraversalObserverT traversal_observer,
            EdgeBreakerObserverT edgebreaker_observer) {
    Init(processor, traversal_observer);
    edgebreaker_observer_ = edgebreaker_observer;
  }
  void TraverseFromCorner(CornerIndex corner_id) {
    corner_traversal_stack_.clear();
    corner_traversal_stack_.push_back(corner_id);
    // For the first face, check the remaining corners as they may not be
    // processed yet.
    const VertexIndex next_vert =
        corner_table_->Vertex(corner_table_->Next(corner_id));
    const VertexIndex prev_vert =
        corner_table_->Vertex(corner_table_->Previous(corner_id));
    if (!processor_.IsVertexVisited(next_vert)) {
      processor_.MarkVertexVisited(next_vert);
      traversal_observer_.OnNewVertexVisited(next_vert,
                                             corner_table_->Next(corner_id));
    }
    if (!processor_.IsVertexVisited(prev_vert)) {
      processor_.MarkVertexVisited(prev_vert);
      traversal_observer_.OnNewVertexVisited(
          prev_vert, corner_table_->Previous(corner_id));
    }

    // Start the actual traversal.
    while (!corner_traversal_stack_.empty()) {
      // Currently processed corner.
      corner_id = corner_traversal_stack_.back();
      FaceIndex face_id(corner_id.value() / 3);
      // Make sure the face hasn't been visited yet.
      if (corner_id < 0 || processor_.IsFaceVisited(face_id)) {
        // This face has been already traversed.
        corner_traversal_stack_.pop_back();
        continue;
      }
      while (true) {
        face_id = FaceIndex(corner_id.value() / 3);
        processor_.MarkFaceVisited(face_id);
        traversal_observer_.OnNewFaceVisited(face_id);
        const VertexIndex vert_id = corner_table_->Vertex(corner_id);
        const bool on_boundary = corner_table_->IsOnBoundary(vert_id);
        if (!processor_.IsVertexVisited(vert_id)) {
          processor_.MarkVertexVisited(vert_id);
          traversal_observer_.OnNewVertexVisited(vert_id, corner_id);
          if (!on_boundary) {
            edgebreaker_observer_.OnSymbolC();
            corner_id = corner_table_->GetRightCorner(corner_id);
            continue;
          }
        }
        // The current vertex has been already visited or it was on a boundary.
        // We need to determine whether we can visit any of it's neighboring
        // faces.
        const CornerIndex right_corner_id =
            corner_table_->GetRightCorner(corner_id);
        const CornerIndex left_corner_id =
            corner_table_->GetLeftCorner(corner_id);
        const FaceIndex right_face_id(
            (right_corner_id < 0 ? -1 : right_corner_id.value() / 3));
        const FaceIndex left_face_id(
            (left_corner_id < 0 ? -1 : left_corner_id.value() / 3));
        if (processor_.IsFaceVisited(right_face_id)) {
          // Right face has been already visited.
          if (processor_.IsFaceVisited(left_face_id)) {
            // Both neighboring faces are visited. End reached.
            edgebreaker_observer_.OnSymbolE();
            corner_traversal_stack_.pop_back();
            break;  // Break from the while (true) loop.
          } else {
            edgebreaker_observer_.OnSymbolR();
            // Go to the left face.
            corner_id = left_corner_id;
          }
        } else {
          // Right face was not visited.
          if (processor_.IsFaceVisited(left_face_id)) {
            edgebreaker_observer_.OnSymbolL();
            // Left face visited, go to the right one.
            corner_id = right_corner_id;
          } else {
            edgebreaker_observer_.OnSymbolS();
            // Both neighboring faces are unvisited, we need to visit both of
            // them.

            // Split the traversal.
            // First make the top of the current corner stack point to the left
            // face (this one will be processed second).
            corner_traversal_stack_.back() = left_corner_id;
            // Add a new corner to the top of the stack (right face needs to
            // be traversed first).
            corner_traversal_stack_.push_back(right_corner_id);
            // Break from the while (true) loop.
            break;
          }
        }
      }
    }
  }

  const CornerTable *corner_table() const { return corner_table_; }
  const TraversalProcessorT &traversal_processor() const { return processor_; }

 private:
  const CornerTable *corner_table_;
  TraversalProcessorT processor_;
  TraversalObserverT traversal_observer_;
  EdgeBreakerObserverT edgebreaker_observer_;
  std::vector<CornerIndex> corner_traversal_stack_;
};

}  // namespace draco

#endif  // DRACO_MESH_EDGEBREAKER_TRAVERSER_H_
