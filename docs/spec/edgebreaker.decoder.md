## EdgeBreaker Decoder

### ParseEdgebreakerConnectivityData()

~~~~~
void ParseEdgebreakerConnectivityData() {
  edgebreaker_traversal_type                                                          UI8
  num_new_vertices                                                                    varUI32
  num_encoded_vertices                                                                varUI32
  num_faces                                                                           varUI32
  num_attribute_data                                                                  UI8
  num_encoded_symbols                                                                 varUI32
  num_encoded_split_symbols                                                           varUI32
}
~~~~~
{:.draco-syntax }


### ParseTopologySplitEvents()

~~~~~
void ParseTopologySplitEvents() {
  num_topology_splits                                                                 varUI32
  for (i = 0; i < num_topology_splits; ++i) {
    source_id_delta[i]                                                                varUI32
    split_id_delta[i]                                                                 varUI32
  }
  for (i = 0; i < num_topology_splits; ++i) {
    split_edge_bit[i]                                                                 f[1]
    source_edge_bit[i]                                                                f[1]
  }
}
~~~~~
{:.draco-syntax }


### DecodeEdgebreakerConnectivityData()

~~~~~
void DecodeEdgebreakerConnectivityData() {
  ParseEdgebreakerConnectivityData();
  DecodeTopologySplitEvents();
  EdgebreakerTraversalStart();
  DecodeEdgeBreakerConnectivity();
}
~~~~~
{:.draco-syntax }


### ProcessSplitData()

~~~~~
void ProcessSplitData() {
  last_id = 0;
  for (i = 0; i < source_id_delta.size(); ++i) {
    source_symbol_id[i] = source_id_delta[i] + last_id;
    split_symbol_id.[i] = source_symbol_id[i] - split_id_delta[i];
    last_id = source_symbol_id[i];
  }
}
~~~~~
{:.draco-syntax }


### DecodeTopologySplitEvents()

~~~~~
void DecodeTopologySplitEvents() {
  ParseTopologySplitEvents();
  ProcessSplitData();
}
~~~~~
{:.draco-syntax }


### IsTopologySplit()

~~~~~
bool IsTopologySplit(encoder_symbol_id, out_face_edge,
                     out_encoder_split_symbol_id) {
  if (source_symbol_id.back() != encoder_symbol_id)
    return false;
  *out_face_edge = source_edge_bit.pop_back();
  *out_encoder_split_symbol_id = split_symbol_id.pop_back();
  source_symbol_id.pop_back();
  return true;
}
~~~~~
{:.draco-syntax }


### ReplaceVerts()

~~~~~
void ReplaceVerts(from, to) {
  for (i = 0; i < face_to_vertex[0].size(); ++i) {
    if (face_to_vertex[0][i] == from) {
      face_to_vertex[0][i] = to;
    }
    if (face_to_vertex[1][i] == from) {
      face_to_vertex[1][i] = to;
    }
    if (face_to_vertex[2][i] == from) {
      face_to_vertex[2][i] = to;
    }
  }
}
~~~~~
{:.draco-syntax }


### UpdateCornersAfterMerge()

~~~~~
void UpdateCornersAfterMerge(c, v) {
  opp_corner = PosOpposite(c);
  if (opp_corner >= 0) {
    corner_n = Next(opp_corner);
    while (corner_n >= 0) {
      MapCornerToVertex(corner_n, v);
      corner_n = SwingLeft(0, corner_n);
    }
  }
}
~~~~~
{:.draco-syntax }


### NewActiveCornerReached()

~~~~~
void NewActiveCornerReached(new_corner, symbol_id) {
  check_topology_split = false;
  switch (last_symbol_) {
    case TOPOLOGY_C:
      {
        corner_a = active_corner_stack.back();
        corner_b = Previous(corner_a);
        while (PosOpposite(corner_b) >= 0) {
          b_opp = PosOpposite(corner_b);
          corner_b = Previous(b_opp);
        }
        SetOppositeCorners(corner_a, new_corner + 1);
        SetOppositeCorners(corner_b, new_corner + 2);
        active_corner_stack.back() = new_corner;
      }
      vert = CornerToVert(Next(corner_a));
      next = CornerToVert(Next(corner_b));
      prev = CornerToVert(Previous(corner_a));
      if (edgebreaker_traversal_type == VALENCE_EDGEBREAKER) {
        vertex_valences_[next] += 1;
        vertex_valences_[prev] += 1;
      }
      face_to_vertex[0].push_back(vert);
      face_to_vertex[1].push_back(next);
      face_to_vertex[2].push_back(prev);
      is_vert_hole_[vert] = false;
      MapCornerToVertex(new_corner, vert);
      MapCornerToVertex(new_corner + 1, next);
      MapCornerToVertex(new_corner + 2, prev);
      break;
    case TOPOLOGY_S:
      {
        corner_b = active_corner_stack.pop_back();
        for (i = 0; i < split_active_corners.size(); ++i) {
          if (split_active_corners[i].decoder_split_symbol_id == symbol_id) {
            active_corner_stack.push_back(split_active_corners[i].corner);
          }
        }
        corner_a = active_corner_stack.back();
        SetOppositeCorners(corner_a, new_corner + 2);
        SetOppositeCorners(corner_b, new_corner + 1);
        active_corner_stack.back() = new_corner;
      }

      vert = CornerToVert(Previous(corner_a));
      next = CornerToVert(Next(corner_a));
      prev = CornerToVert(Previous(corner_b));
      MapCornerToVertex(new_corner, vert);
      MapCornerToVertex(new_corner + 1, next);
      MapCornerToVertex(new_corner + 2, prev);
      corner_n = Next(corner_b);
      vertex_n = CornerToVert(corner_n);
      if (edgebreaker_traversal_type == VALENCE_EDGEBREAKER) {
        vertex_valences_[vert] += vertex_valences_[vertex_n];
      }
      ReplaceVerts(vertex_n, vert);
      if (edgebreaker_traversal_type == VALENCE_EDGEBREAKER) {
        vertex_valences_[next] += 1;
        vertex_valences_[prev] += 1;
      }
      face_to_vertex[0].push_back(vert);
      face_to_vertex[1].push_back(next);
      face_to_vertex[2].push_back(prev);
      UpdateCornersAfterMerge(new_corner + 1, vert);
      vertex_corners_[vertex_n] = kInvalidCornerIndex;
      break;
    case TOPOLOGY_R:
      {
        corner_a = active_corner_stack.back();
        opp_corner = new_corner + 2;
        SetOppositeCorners(opp_corner, corner_a);
        active_corner_stack.back() = new_corner;
      }
      check_topology_split = true;
      vert = CornerToVert(Previous(corner_a));
      next = CornerToVert(Next(corner_a));
      prev = ++last_vert_added;
      if (edgebreaker_traversal_type == VALENCE_EDGEBREAKER) {
        vertex_valences_[vert] += 1;
        vertex_valences_[next] += 1;
        vertex_valences_[prev] += 2;
      }

      face_to_vertex[0].push_back(vert);
      face_to_vertex[1].push_back(next);
      face_to_vertex[2].push_back(prev);

      MapCornerToVertex(new_corner + 2, prev);
      MapCornerToVertex(new_corner, vert);
      MapCornerToVertex(new_corner + 1, next);
      break;
    case TOPOLOGY_L:
      {
        corner_a = active_corner_stack.back();
        opp_corner = new_corner + 1;
        SetOppositeCorners(opp_corner, corner_a);
        active_corner_stack.back() = new_corner;
      }
      check_topology_split = true;
      vert = CornerToVert(Previous(corner_a));
      next = CornerToVert(Next(corner_a));
      prev = ++last_vert_added;
      if (edgebreaker_traversal_type == VALENCE_EDGEBREAKER) {
        vertex_valences_[vert] += 1;
        vertex_valences_[next] += 1;
        vertex_valences_[prev] += 2;
      }

      face_to_vertex[0].push_back(vert);
      face_to_vertex[1].push_back(next);
      face_to_vertex[2].push_back(prev);

      MapCornerToVertex(new_corner + 2, prev);
      MapCornerToVertex(new_corner, vert);
      MapCornerToVertex(new_corner + 1, next);
      break;
    case TOPOLOGY_E:
      active_corner_stack.push_back(new_corner);
      check_topology_split = true;
      vert = last_vert_added + 1;
      next = vert + 1;
      prev = next + 1;
      if (edgebreaker_traversal_type == VALENCE_EDGEBREAKER) {
        vertex_valences_[vert] += 2;
        vertex_valences_[next] += 2;
        vertex_valences_[prev] += 2;
      }
      face_to_vertex[0].push_back(vert);
      face_to_vertex[1].push_back(next);
      face_to_vertex[2].push_back(prev);
      last_vert_added = prev;
      MapCornerToVertex(new_corner, vert);
      MapCornerToVertex(new_corner + 1, next);
      MapCornerToVertex(new_corner + 2, prev);
      break;
  }

  if (edgebreaker_traversal_type == VALENCE_EDGEBREAKER) {
    // Compute the new context that is going to be used
    // to decode the next symbol.
    active_valence = vertex_valences_[next];
    if (active_valence < MIN_VALENCE) {
      clamped_valence = MIN_VALENCE;
    } else if (active_valence > MAX_VALENCE) {
      clamped_valence = MAX_VALENCE;
    } else {
      clamped_valence = active_valence;
    }
    active_context_ = (clamped_valence - MIN_VALENCE);
  }

  if (check_topology_split) {
    encoder_symbol_id = num_encoded_symbols - symbol_id - 1;
    while (IsTopologySplit(encoder_symbol_id, &split_edge,
                           &enc_split_id)) {
      act_top_corner = active_corner_stack.back();
      if (split_edge == RIGHT_FACE_EDGE) {
        new_active_corner = Next(act_top_corner);
      } else {
        new_active_corner = Previous(act_top_corner);
      }
      // Convert the encoder split symbol id to decoder symbol id.
      dec_split_id = num_encoded_symbols - enc_split_id - 1;
      topology_edge_list[dec_split_id] = new_active_corner;
    }
  }
}
~~~~~
{:.draco-syntax }


### ParseEdgebreakerStandardSymbol()

~~~~~
void ParseEdgebreakerStandardSymbol() {
  symbol = bit_symbol_buffer.ReadBits32(1);
  if (symbol != TOPOLOGY_C) {
    // Else decode two additional bits.
    symbol_suffix = bit_symbol_buffer.ReadBits(2);
    symbol |= (symbol_suffix << 1);
  }
  last_symbol_ = symbol;
}
~~~~~
{:.draco-syntax }


### EdgebreakerDecodeSymbol()

~~~~~
void EdgebreakerDecodeSymbol() {
  if (edgebreaker_traversal_type == VALENCE_EDGEBREAKER) {
    EdgebreakerValenceDecodeSymbol(sym);
  } else if (edgebreaker_traversal_type == STANDARD_EDGEBREAKER) {
    ParseEdgebreakerStandardSymbol(sym);
  }
}
~~~~~
{:.draco-syntax }


### DecodeEdgeBreakerConnectivity()

~~~~~
void DecodeEdgeBreakerConnectivity() {
  is_vert_hole_.assign(num_encoded_vertices + num_encoded_split_symbols, true);
  for (i = 0; i < num_encoded_symbols; ++i) {
    EdgebreakerDecodeSymbol();
    corner = 3 * i;
    NewActiveCornerReached(corner, i);
  }
  ProcessInteriorEdges();
}
~~~~~
{:.draco-syntax }
