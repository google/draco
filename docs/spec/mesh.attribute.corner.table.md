
## Mesh Attribute Corner Table

### bool IsCornerOnSeam()

~~~~~
bool IsCornerOnSeam(corner) {
  return is_vertex_on_seam_[corner_table_->Vertex(corner)];
}
~~~~~
{:.draco-syntax }


### AddSeamEdge()

~~~~~
AddSeamEdge(c) {
  MarkSeam(c)
  opp_corner = corner_table_->Opposite(c);
  if (opp_corner >= 0) {
    no_interior_seams_ = false;
    MarkSeam(opp_corner)
  }
}
~~~~~
{:.draco-syntax }


### MarkSeam()

~~~~~
MarkSeam(c) {
  is_edge_on_seam_[c] = true;
  is_vertex_on_seam_[corner_table_->Vertex(corner_table_->Next(c))] = true;
  is_vertex_on_seam_[corner_table_->Vertex(corner_table_->Previous(c))
                         ] = true;
}
~~~~~
{:.draco-syntax }


### RecomputeVertices()

~~~~~
RecomputeVertices() {
  // in code RecomputeVerticesInternal<false>(nullptr, nullptr)
  num_new_vertices = 0;
  for (v = 0; v < corner_table_->num_vertices(); ++v) {
    c = corner_table_->LeftMostCorner(v);
    if (c < 0)
      continue;
    first_vert_id(num_new_vertices++);
    vertex_to_attribute_entry_id_map_.push_back(first_vert_id);
    first_c = c;
    if (is_vertex_on_seam_[v]) {
      act_c = SwingLeft(first_c);
      while (act_c >= 0) {
        first_c = act_c;
        act_c = SwingLeft(act_c);
      }
    }
    corner_to_vertex_map_[first_c] =first_vert_id;
    vertex_to_left_most_corner_map_.push_back(first_c);
    act_c = corner_table_->SwingRight(first_c);
    while (act_c >= 0 && act_c != first_c) {
      if (is_edge_on_seam_[corner_table_->Next(act_c)]) {
        // in code IsCornerOppositeToSeamEdge()
        first_vert_id = AttributeValueIndex(num_new_vertices++);
        vertex_to_attribute_entry_id_map_.push_back(first_vert_id);
        vertex_to_left_most_corner_map_.push_back(act_c);
      }
      corner_to_vertex_map_[act_c] = first_vert_id;
      act_c = corner_table_->SwingRight(act_c);
    }
  }
}
~~~~~
{:.draco-syntax }
