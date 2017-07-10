
## CornerTable Traversal Processor


### IsFaceVisited()

~~~~~
IsFaceVisited(corner_id) {
  if (corner_id < 0)
    return true
  return is_face_visited_[corner_id / 3];
}
~~~~~


### MarkFaceVisited()

~~~~~
MarkFaceVisited(face_id) {
  is_face_visited_[face_id] = true;
}
~~~~~


### IsVertexVisited()

~~~~~
IsVertexVisited(vert_id) {
  return is_vertex_visited_[vert_id];
}
~~~~~


### MarkVertexVisited()

~~~~~
MarkVertexVisited(vert_id) {
  is_vertex_visited_[vert_id] = true;
}
~~~~~
