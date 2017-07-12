
## Mesh Prediction Scheme Parallelogram

### Decode()

~~~~~
Decode(...) {
  this->transform().InitializeDecoding(num_components);
  // restore the first value
  this->transform().ComputeOriginalValue(pred_vals.get(),
                                         in_corr, out_data, 0);
  // PredictionSchemeWrapTransform_ComputeOriginalValue()
  corner_map_size = this->mesh_data().data_to_corner_map()->size();
  for (p = 1; p < corner_map_size; ++p) {
    corner_id = this->mesh_data().data_to_corner_map()->at(p);
    dst_offset = p * num_components;
    b= ComputeParallelogramPrediction(p, corner_id, table,
                                        *vertex_to_data_map, out_data,
                                        num_components, pred_vals.get())
    if (!b) {
      src_offset = (p - 1) * num_components;
      this->transform().ComputeOriginalValue(out_data + src_offset, in_corr,
                                             out_data + dst_offset, dst_offset);
      // PredictionSchemeWrapTransform_ComputeOriginalValue()
    } else {
      this->transform().ComputeOriginalValue(pred_vals.get(), in_corr,
                                             out_data + dst_offset, dst_offset);
      // PredictionSchemeWrapTransform_ComputeOriginalValue()
    }
  }
}
~~~~~
{:.draco-syntax }


MeshPredictionSchemeParallelogramShared

FIXME: ^^^ Heading level?


### ComputeParallelogramPrediction()

~~~~~
ComputeParallelogramPrediction(...) {
  oci = table->Opposite(ci);
  vert_opp = vertex_to_data_map[table->Vertex(ci)];
  vert_next = vertex_to_data_map[table->Vertex(table->Next(ci))];
  vert_prev = vertex_to_data_map[table->Vertex(table->Previous(ci))];
  if (vert_opp < data_entry_id && vert_next < data_entry_id &&
      vert_prev < data_entry_id) {
    v_opp_off = vert_opp * num_components;
    v_next_off = vert_next * num_components;
    v_prev_off = vert_prev * num_components;
    for (c = 0; c < num_components; ++c) {
      out_prediction[c] = (in_data[v_next_off + c] + in_data[v_prev_off + c]) -
                          in_data[v_opp_off + c];
    }
    Return true;
  }
  return false;
}
~~~~~
{:.draco-syntax }
