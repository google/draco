
## Mesh Traversal Sequencer

### GenerateSequenceInternal()

~~~~~
GenerateSequenceInternal() {
  traverser_.OnTraversalStart();
   If (corner_order_) {
    // TODO
  } else {
    int32_t num_faces = traverser_.corner_table()->num_faces();
    for (i = 0; i < num_faces; ++i) {
      ProcessCorner(3 * i)
    }
  }
  traverser_.OnTraversalEnd();
}
~~~~~
{:.draco-syntax }


### ProcessCorner()

~~~~~
ProcessCorner(corner_id) {
  traverser_.TraverseFromCorner(corner_id);
}
~~~~~
{:.draco-syntax }


### UpdatePointToAttributeIndexMapping()

~~~~~
UpdatePointToAttributeIndexMapping(PointAttribute *attribute) {
  corner_table = traverser_.corner_table();
  attribute->SetExplicitMapping(mesh_->num_points());
  num_faces = mesh_->num_faces();
  num_points = mesh_->num_points();
  for (f = 0; f < num_faces; ++f) {
    face = mesh_->face(f);
    for (p = 0; p < 3; ++p) {
      point_id = face[p];
      vert_id = corner_table->Vertex(3 * f + p);
      att_entry_id(
            encoding_data_
                ->vertex_to_encoded_attribute_value_index_map[vert_id]);
      attribute->SetPointMapEntry(point_id, att_entry_id);
    }
  }
}
~~~~~
{:.draco-syntax }


PointsSequencer

FIXME: ^^^ Heading level?

### AddPointId()

~~~~~
AddPointId(point_id) {
  out_point_ids_->push_back(point_id);
}
~~~~~
{:.draco-syntax }
