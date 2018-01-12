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
#ifndef DRACO_MESH_PREDICTION_DEGREE_TRAVERSER_H_
#define DRACO_MESH_PREDICTION_DEGREE_TRAVERSER_H_

#include <vector>

#include "draco/mesh/corner_table.h"

namespace draco {

// PredictionDegreeTraverser provides framework for traversal over a corner
// table data structure following paper "Multi-way Geometry Encoding" by
// Cohen-or at al.'02. The traversal is implicitly guided by prediction degree
// of the destination vertices. A prediction degree is computed as the number of
// possible faces that can be used as source points for traversal to the given
// destination vertex (see image below, where faces F1 and F2 are already
// traversed and face F0 is not traversed yet. The prediction degree of vertex
// V is then equal to two).
//
//            X-----V-----o
//           / \   / \   / \
//          / F0\ /   \ / F2\
//         X-----o-----o-----B
//                \ F1/
//                 \ /
//                  A
//
// The class implements the same interface as the EdgebreakerTraverser
// (edgebreaker_traverser.h) and it can be controlled via the same template
// trait classes |TraversalProcessorT| and |TraversalObserverT|, that are used
// for controlling and monitoring of the traversal respectively. For details,
// please see edgebreaker_traverser.h.
template <class TraversalProcessorT, class TraversalObserverT>
class PredictionDegreeTraverser {
 public:
  typedef TraversalProcessorT TraversalProcessor;
  typedef TraversalObserverT TraversalObserver;
  typedef typename TraversalProcessorT::CornerTable CornerTable;

  PredictionDegreeTraverser() {}
  void Init(TraversalProcessorT &&processor) {
    corner_table_ = &processor.GetCornerTable();
    processor_ = std::move(processor);
  }
  void Init(TraversalProcessorT &&processor,
            TraversalObserverT traversal_observer) {
    Init(std::move(processor));
    traversal_observer_ = traversal_observer;
  }

  // Called before any traversing starts.
  void OnTraversalStart() {
    prediction_degree_.resize(corner_table_->num_vertices(), 0);
  }

  // Called when all the traversing is done.
  void OnTraversalEnd() {}

  void TraverseFromCorner(CornerIndex corner_id) {
    if (prediction_degree_.size() == 0)
      return;

    // Traversal starts from the |corner_id|. It's going to follow either the
    // right or the left neighboring faces to |corner_id| based on their
    // prediction degree.
    traversal_stacks_[0].push_back(corner_id);
    best_priority_ = 0;
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
    const VertexIndex tip_vertex = corner_table_->Vertex(corner_id);
    if (!processor_.IsVertexVisited(tip_vertex)) {
      processor_.MarkVertexVisited(tip_vertex);
      traversal_observer_.OnNewVertexVisited(tip_vertex, corner_id);
    }
    // Start the actual traversal.
    while ((corner_id = PopNextCornerToTraverse()) != kInvalidCornerIndex) {
      FaceIndex face_id(corner_id.value() / 3);
      // Make sure the face hasn't been visited yet.
      if (processor_.IsFaceVisited(face_id)) {
        // This face has been already traversed.
        continue;
      }

      while (true) {
        face_id = FaceIndex(corner_id.value() / 3);
        processor_.MarkFaceVisited(face_id);
        traversal_observer_.OnNewFaceVisited(face_id);

        // If the newly reached vertex hasn't been visited, mark it and notify
        // the observer.
        const VertexIndex vert_id = corner_table_->Vertex(corner_id);
        if (!processor_.IsVertexVisited(vert_id)) {
          processor_.MarkVertexVisited(vert_id);
          traversal_observer_.OnNewVertexVisited(vert_id, corner_id);
        }

        // Check whether we can traverse to the right and left neighboring
        // faces.
        const CornerIndex right_corner_id =
            corner_table_->GetRightCorner(corner_id);
        const CornerIndex left_corner_id =
            corner_table_->GetLeftCorner(corner_id);
        const FaceIndex right_face_id(
            (right_corner_id == kInvalidCornerIndex
                 ? kInvalidFaceIndex
                 : FaceIndex(right_corner_id.value() / 3)));
        const FaceIndex left_face_id(
            (left_corner_id == kInvalidCornerIndex
                 ? kInvalidFaceIndex
                 : FaceIndex(left_corner_id.value() / 3)));
        const bool is_right_face_visited =
            processor_.IsFaceVisited(right_face_id);
        const bool is_left_face_visited =
            processor_.IsFaceVisited(left_face_id);

        if (!is_left_face_visited) {
          // We can go to the left face.
          const int priority = ComputePriority(left_corner_id);
          if (is_right_face_visited && priority <= best_priority_) {
            // Right face has been already visited and the priority is equal or
            // better than the best priority. We are sure that the left face
            // would be traversed next so there is no need to put it onto the
            // stack.
            corner_id = left_corner_id;
            continue;
          } else {
            AddCornerToTraversalStack(left_corner_id, priority);
          }
        }
        if (!is_right_face_visited) {
          // Go to the right face.
          const int priority = ComputePriority(right_corner_id);
          if (priority <= best_priority_) {
            // We are sure that the right face would be traversed next so there
            // is no need to put it onto the stack.
            corner_id = right_corner_id;
            continue;
          } else {
            AddCornerToTraversalStack(right_corner_id, priority);
          }
        }

        // Couldn't proceed directly to the next corner
        break;
      }
    }
  }

  const CornerTable *corner_table() const { return corner_table_; }
  const TraversalProcessorT &traversal_processor() const { return processor_; }
  const TraversalObserverT &traversal_observer() const {
    return traversal_observer_;
  }

 private:
  // Retrieves the next available corner (edge) to traverse. Edges are processed
  // based on their priorities.
  // Returns kInvalidCornerIndex when there is no edge available.
  CornerIndex PopNextCornerToTraverse() {
    for (int i = best_priority_; i < kMaxPriority; ++i) {
      if (!traversal_stacks_[i].empty()) {
        const CornerIndex ret = traversal_stacks_[i].back();
        traversal_stacks_[i].pop_back();
        best_priority_ = i;
        return ret;
      }
    }
    return kInvalidCornerIndex;
  }

  inline void AddCornerToTraversalStack(CornerIndex ci, int priority) {
    traversal_stacks_[priority].push_back(ci);
    // Make sure that the best available priority is up to date.
    if (priority < best_priority_)
      best_priority_ = priority;
  }

  // Returns the priority of traversing edge leading to |corner_id|.
  inline int ComputePriority(CornerIndex corner_id) {
    const VertexIndex v_tip = corner_table_->Vertex(corner_id);
    // Priority 0 when traversing to already visited vertices.
    int priority = 0;
    if (!processor_.IsVertexVisited(v_tip)) {
      const int degree = ++prediction_degree_[v_tip];
      // Priority 1 when prediction degree > 1, otherwise 2.
      priority = (degree > 1 ? 1 : 2);
    }
    // Clamp the priority to the maximum number of buckets.
    if (priority >= kMaxPriority)
      priority = kMaxPriority - 1;
    return priority;
  }

  const CornerTable *corner_table_;
  TraversalProcessorT processor_;
  TraversalObserverT traversal_observer_;

  // For efficiency reasons, the priority traversal is implemented using buckets
  // where each buckets represent a stack of available corners for a given
  // priority. Corners with the highest priority are always processed first.
  static constexpr int kMaxPriority = 3;
  std::vector<CornerIndex> traversal_stacks_[kMaxPriority];

  // Keep the track of the best available priority to improve the performance
  // of PopNextCornerToTraverse() method.
  int best_priority_;

  // Prediction degree available for each vertex.
  IndexTypeVector<VertexIndex, int> prediction_degree_;
};

}  // namespace draco

#endif  // DRACO_MESH_PREDICTION_DEGREE_TRAVERSER_H_
