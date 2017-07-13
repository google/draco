
## Mesh Attribute Indices Encoding Observer

### OnNewVertexVisited()

~~~~~
OnNewVertexVisited(vertex, corner) {
  point_id = mesh_->face(corner / 3)[corner % 3];
  sequencer_->AddPointId(point_id);
  // Keep track of visited corners.
  encoding_data_->encoded_attribute_value_index_to_corner_map.push_back(corner);
  encoding_data_
        ->vertex_to_encoded_attribute_value_index_map[vertex] =
        encoding_data_->num_values;
  encoding_data_->num_values++;
}
~~~~~
{:.draco-syntax }
