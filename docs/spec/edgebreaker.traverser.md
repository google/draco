
## EdgeBreaker Traverser

### TraverseFromCorner()

~~~~~
TraverseFromCorner(corner_id) {
  if (processor_.IsFaceVisited(corner_id))
    return
  corner_traversal_stack_.clear();
  corner_traversal_stack_.push_back(corner_id);
  next_vert = corner_table_->Vertex(corner_table_->Next(corner_id));
  prev_vert = corner_table_->Vertex(corner_table_->Previous(corner_id));
  if (!processor_.IsVertexVisited(next_vert)) {
    processor_.MarkVertexVisited(next_vert);
    traversal_observer_.OnNewVertexVisited(next_vert,
                                        corner_table_->Next(corner_id));
  }
  if (!processor_.IsVertexVisited(prev_vert)) {
    processor_.MarkVertexVisited(prev_vert);
    traversal_observer_.OnNewVertexVisited(prev_vert,
                                        corner_table_->Previous(corner_id));
  }
  while (!corner_traversal_stack_.empty()) {
    corner_id = corner_traversal_stack_.back();
    face_id =corner_id / 3;
    if (processor_.IsFaceVisited(face_id)) {
      corner_traversal_stack_.pop_back();
      continue
    }
    while(true) {
      face_id = corner_id / 3;
      processor_.MarkFaceVisited(face_id);
      traversal_observer_.OnNewFaceVisited(face_id);
      vert_id = corner_table_->Vertex(corner_id);
      on_boundary = corner_table_->IsOnBoundary(vert_id);
      if (!processor_.IsVertexVisited(vert_id)) {
        processor_.MarkVertexVisited(vert_id);
        traversal_observer_.OnNewVertexVisited(vert_id, corner_id);
        if (!on_boundary) {
          corner_id = corner_table_->GetRightCorner(corner_id);
          continue;
        }
      }
      // The current vertex has been already visited or it was on a boundary.
      right_corner_id = corner_table_->GetRightCorner(corner_id);
      left_corner_id = corner_table_->GetLeftCorner(corner_id);
      right_face_id((right_corner_id < 0 ? -1 : right_corner_id / 3));
      left_face_id((left_corner_id < 0 ? -1 : left_corner_id / 3));
      if (processor_.IsFaceVisited(right_face_id)) {
        if (processor_.IsFaceVisited(left_face_id)) {
          corner_traversal_stack_.pop_back();
          break; // Break from while(true) loop
        } else {
          corner_id = left_corner_id;
        }
      } else {
        if (processor_.IsFaceVisited(left_face_id)) {
          corner_id = right_corner_id;
       } else {
          // Split the traversal.
          corner_traversal_stack_.back() = left_corner_id;
          corner_traversal_stack_.push_back(right_corner_id);
          break; // Break from while(true) loop
        }
      }
    }
  }
}
~~~~~
{:.draco-syntax }
