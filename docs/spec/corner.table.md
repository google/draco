
## Corner Table

### Opposite()

~~~~~
Opposite(corner) {
  return opposite_corners_[corner];
}
~~~~~


### Next()

~~~~~
Next(corner) {
  return LocalIndex(++corner) ? corner : corner - 3;
}
~~~~~


### Previous()

~~~~~
Previous(corner) {
  return LocalIndex(corner) ? corner - 1 : corner + 2;
}
~~~~~


### Vertex()

~~~~~
Vertex(corner) {
  faces_[Face(corner)][LocalIndex(corner)];
}
~~~~~


### Face()

~~~~~
Face(corner) {
  return corner / 3;
}
~~~~~


### LocalIndex()

~~~~~
LocalIndex(corner) {
  return corner % 3;
}
~~~~~


### num_vertices()

~~~~~
num_vertices() {
  return vertex_corners_.size();
}
~~~~~


### num_corners()

~~~~~
num_corners() {
  return faces_.size() * 3;
}
~~~~~


### num_faces()

~~~~~
num_faces() {
  return faces_.size();
}
~~~~~


### bool IsOnBoundary()

~~~~~
bool IsOnBoundary(vert) {
  corner = LeftMostCorner(vert);
  if (SwingLeft(corner) < 0)
    return true;
  return false;
}
~~~~~



### SwingRight()

~~~~~
SwingRight(corner) {
  return Previous(Opposite(Previous(corner)));
}
~~~~~


### SwingLeft()

~~~~~
SwingLeft(corner) {
  return Next(Opposite(Next(corner)));
}
~~~~~


### GetLeftCorner()

~~~~~
GetLeftCorner(corner_id) {
  if (corner_id < 0)
     return kInvalidCornerIndex;
  return Opposite(Previous(corner_id));
}
~~~~~


### GetRightCorner()

~~~~~
GetRightCorner(corner_id) {
  if (corner_id < 0)
     return kInvalidCornerIndex;
  return Opposite(Next(corner_id));
}
~~~~~


### SetOppositeCorner()

~~~~~
SetOppositeCorner(corner_id, pp_corner_id) {
  opposite_corners_[corner_id] = opp_corner_id;
}
~~~~~



### MapCornerToVertex()

~~~~~
MapCornerToVertex(corner_id, vert_id) {
  face = Face(corner_id);
  faces_[face][LocalIndex(corner_id)] = vert_id;
  if (vert_id >= 0) {
    vertex_corners_[vert_id] = corner_id;
  }
}
~~~~~


### UpdateVertexToCornerMap()

~~~~~
UpdateVertexToCornerMap(vert) {
  first_c = vertex_corners_[vert];
  if (first_c < 0)
    return;
  act_c = SwingLeft(first_c);
  c = first_c;
  while (act_c >= 0 && act_c != first_c) {
    c = act_c;
    act_c = SwingLeft(act_c);
  }
  if (act_c != first_c) {
    vertex_corners_[vert] = c;
  }
}
~~~~~



### LeftMostCorner()

~~~~~
LeftMostCorner(v) {
  return vertex_corners_[v];
}
~~~~~


### MakeVertexIsolated()

~~~~~
MakeVertexIsolated(vert) {
  vertex_corners_[vert] = kInvalidCornerIndex;
}
~~~~~
