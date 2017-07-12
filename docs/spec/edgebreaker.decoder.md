## EdgeBreaker Decoder

### InitializeDecoder()

~~~~~
InitializeDecoder() {
  edgebreaker_decoder_type                                              UI8
}
~~~~~
{:.draco-syntax }


### DecodeConnectivity()

~~~~~
DecodeConnectivity() {
  num_new_verts                                                         UI32
  num_encoded_vertices                                                  UI32
  num_faces                                                             UI32
  num_attribute_data                                                    I8
  num_encoded_symbols                                                   UI32
  num_encoded_split_symbols                                             UI32
  encoded_connectivity_size                                             UI32
  // file pointer must be set to current position
  // + encoded_connectivity_size
  hole_and_split_bytes = DecodeHoleAndTopologySplitEvents()
  // file pointer must be set to old current position
  EdgeBreakerTraversalValence_Start()
  DecodeConnectivity(num_symbols)
  if (attribute_data_.size() > 0) {
    for (ci = 0; ci < corner_table_->num_corners(); ci += 3) {
      DecodeAttributeConnectivitiesOnFace(ci)
    }
  }
  for (i = 0; i < corner_table_->num_vertices(); ++i) {
    if (is_vert_hole_[i]) {
      corner_table_->UpdateVertexToCornerMap(i);
    }
  }
  // Decode attribute connectivity.
  for (uint32_t i = 0; i < attribute_data_.size(); ++i) {
    attribute_data_[i].connectivity_data.InitEmpty(corner_table_.get());
    for (int32_t c : attribute_data_[i].attribute_seam_corners) {
      attribute_data_[i].connectivity_data.AddSeamEdge(c);
    }
    attribute_data_[i].connectivity_data.RecomputeVertices(nullptr, nullptr);
  }
  // Preallocate vertex to value mapping
  AssignPointsToCorners()
}
~~~~~
{:.draco-syntax }


### AssignPointsToCorners()

~~~~~
AssignPointsToCorners() {
  decoder_->mesh()->SetNumFaces(corner_table_->num_faces());
  if (attribute_data_.size() == 0) {
    for (f = 0; f < decoder_->mesh()->num_faces(); ++f) {
      for (c = 0; c < 3; ++c) {
        vert_id = corner_table_->Vertex(3 * f + c);
        if (point_id == -1)
          point_id = num_points++;
        face[c] = point_id;
      }
      decoder_->mesh()->SetFace(f, face);
    }
    decoder_->point_cloud()->set_num_points(num_points);
    Return true;
  }
  for (v = 0; v < corner_table_->num_vertices(); ++v) {
    c = corner_table_->LeftMostCorner(v);
    if (c < 0)
      continue;
    deduplication_first_corner = c;
    if (!is_vert_hole_[v]) {
      for (uint32_t i = 0; i < attribute_data_.size(); ++i) {
        if (!attribute_data_[i].connectivity_data.IsCornerOnSeam(c))
          continue;
        vert_id = attribute_data_[i].connectivity_data.Vertex(c);
        act_c = corner_table_->SwingRight(c);
        seam_found = false;
        while (act_c != c) {
          if (attribute_data_[i].connectivity_data.Vertex(act_c) != vert_id) {
            deduplication_first_corner = act_c;
            seam_found = true;
            break;
          }
          act_c = corner_table_->SwingRight(act_c);
        }
        if (seam_found)
          break;
      }
    }
    c = deduplication_first_corner;
    corner_to_point_map[c] = point_to_corner_map.size();
    point_to_corner_map.push_back(c);
    prev_c = c;
    c = corner_table_->SwingRight(c);
    while (c >= 0 && c != deduplication_first_corner) {
      attribute_seam = false;
      for (uint32_t i = 0; i < attribute_data_.size(); ++i) {
        if (attribute_data_[i].connectivity_data.Vertex(c) !=
            attribute_data_[i].connectivity_data.Vertex(prev_c)) {
          attribute_seam = true;
          break;
        }
      }
      if (attribute_seam) {
        corner_to_point_map[c] = point_to_corner_map.size();
        point_to_corner_map.push_back(c);
      } else {
        corner_to_point_map[c] = corner_to_point_map[prev_c];
      }
      prev_c = c;
      c = corner_table_->SwingRight(c);
    }
  }
  for (f = 0; f < decoder_->mesh()->num_faces(); ++f) {
    for (c = 0; c < 3; ++c) {
      face[c] = corner_to_point_map[3 * f + c];
    }
    decoder_->mesh()->SetFace(f, face);
  }
  decoder_->point_cloud()->set_num_points(point_to_corner_map.size());
}
~~~~~
{:.draco-syntax }


### DecodeConnectivity()

~~~~~
DecodeConnectivity(num_symbols) {
  for (i = 0; i < num_symbols; ++i) {
    symbol = TraversalValence_DecodeSymbol()
    corner = 3 * num_faces++
    if (symbol == TOPOLOGY_C) {
      vertex_x = UpdateCornerTableForSymbolC()
      is_vert_hole_[vertex_x] = false;
    } else if  (symbol == TOPOLOGY_R || symbol == TOPOLOGY_L) {
      UpdateCornerTableForSymbolLR()
      check_topology_split = true;
    } else if  (symbol == TOPOLOGY_S) {
      HandleSymbolS()
    } else if  (symbol == TOPOLOGY_E) {
      UpdateCornerTableForSymbolE()
      check_topology_split = true;
    }
    active_corner_stack.back() = corner;
  traversal_decoder_.NewActiveCornerReached(corner);
    if (check_topology_split) {
      encoder_symbol_id = num_symbols - symbol_id - 1;
      while (true) {
        split = IsTopologySplit(encoder_symbol_id, &split_edge,
                             &encoder_split_symbol_id);
        if (!split) {
          break;
        }
        act_top_corner = corner;
        if (split_edge == RIGHT_FACE_EDGE) {
          new_active_corner = corner_table_->Next(act_top_corner);
        } else {
          new_active_corner = corner_table_->Previous(act_top_corner);
        }
        decoder_split_symbol_id = num_symbols - encoder_split_symbol_id - 1;
        topology_split_active_corners[decoder_split_symbol_id] =
            new_active_corner;
      }
    }
  }
  while (active_corner_stack.size() > 0) {
    corner = active_corner_stack.pop_back();
    interior_face = traversal_decoder_.DecodeStartFaceConfiguration();
    if (interior_face == true) {
      UpdateCornerTableForInteriorFace()
      for (ci = 0; ci < 3; ++ci) {
        is_vert_hole_[corner_table_->Vertex(new_corner + ci)] = false;
      }
      init_face_configurations_.push_back(true);
      init_corners_.push_back(new_corner);
    } else {
      init_face_configurations_.push_back(false);
      init_corners_.push_back(corner);
    }
  }
  Return num_vertices;
}
~~~~~
{:.draco-syntax }


### UpdateCornerTableForSymbolC()

~~~~~
UpdateCornerTableForSymbolC(corner) {
  corner_a = active_corner_stack.back();
  corner_b = corner_table_->Previous(corner_a);
  while (corner_table_->Opposite(corner_b) >= 0) {
    corner_b = corner_table_->Previous(corner_table_->Opposite(corner_b));
  }
  SetOppositeCorners(corner_a, corner + 1);
  SetOppositeCorners(corner_b, corner + 2);
  vertex_x = corner_table_->Vertex(corner_table_->Next(corner_a));
  corner_table_->MapCornerToVertex(corner, vertex_x);
  corner_table_->MapCornerToVertex(
          corner + 1, corner_table_->Vertex(corner_table_->Next(corner_b)));
  corner_table_->MapCornerToVertex(
          corner + 2, corner_table_->Vertex(corner_table_->Previous(corner_a)));
  return vertex_x;
}
~~~~~
{:.draco-syntax }



### UpdateCornerTableForSymbolLR()

~~~~~
UpdateCornerTableForSymbolLR(corner, symbol) {
  if (symbol == TOPOLOGY_R) {
    opp_corner = corner + 2;
  } else {
    opp_corner = corner + 1;
  }
  SetOppositeCorners(opp_corner, corner_a);
  corner_table_->MapCornerToVertex(opp_corner,num_vertices++);
  corner_table_->MapCornerToVertex(
          corner_table_->Next(opp_corner),
          corner_table_->Vertex(corner_table_->Previous(corner_a)));
  corner_table_->MapCornerToVertex(
          corner_table_->Previous(opp_corner),
          corner_table_->Vertex(corner_table_->Next(corner_a)));
}
~~~~~
{:.draco-syntax }


### HandleSymbolS()

~~~~~
HandleSymbolS(corner) {
  corner_b = active_corner_stack.pop_back();
  it = topology_split_active_corners.find(symbol_id);
  if (it != topology_split_active_corners.end()) {
    active_corner_stack.push_back(it->second);
  }
  corner_a = active_corner_stack.back();
  SetOppositeCorners(corner_a, corner + 2);
  SetOppositeCorners(corner_b, corner + 1);
  vertex_p = corner_table_->Vertex(corner_table_->Previous(corner_a));
  corner_table_->MapCornerToVertex(corner, vertex_p);
  corner_table_->MapCornerToVertex(
          corner + 1, corner_table_->Vertex(corner_table_->Next(corner_a)));
  corner_table_->MapCornerToVertex(corner + 2,
           corner_table_->Vertex(corner_table_->Previous(corner_b)));
  corner_n = corner_table_->Next(corner_b);
  vertex_n = corner_table_->Vertex(corner_n);
  traversal_decoder_.MergeVertices(vertex_p, vertex_n);
  // TraversalValence_MergeVertices
  while (corner_n >= 0) {
    corner_table_->MapCornerToVertex(corner_n, vertex_p);
    corner_n = corner_table_->SwingLeft(corner_n);
  }
  corner_table_->MakeVertexIsolated(vertex_n);
}
~~~~~
{:.draco-syntax }


### UpdateCornerTableForSymbolE()

~~~~~
UpdateCornerTableForSymbolE() {
  corner_table_->MapCornerToVertex(corner, num_vertices++);
  corner_table_->MapCornerToVertex(corner + 1, num_vertices++);
  corner_table_->MapCornerToVertex(corner + 2, num_vertices++);
}
~~~~~
{:.draco-syntax }


### UpdateCornerTableForInteriorFace()

~~~~~
UpdateCornerTableForInteriorFace() {
  corner_b = corner_table_->Previous(corner);
  while (corner_table_->Opposite(corner_b) >= 0) {
    corner_b = corner_table_->Previous(corner_table_->Opposite(corner_b));
  }
  corner_c = corner_table_->Next(corner);
  while (corner_table_->Opposite(corner_c) >= 0) {
    corner_c = corner_table_->Next(corner_table_->Opposite(corner_c));
  }
  face(num_faces++);
  corner_table_->MapCornerToVertex(
          new_corner, corner_table_->Vertex(corner_table_->Next(corner_b)));
  corner_table_->MapCornerToVertex(
          new_corner + 1, corner_table_->Vertex(corner_table_->Next(corner_c)));
  corner_table_->MapCornerToVertex(
          new_corner + 2, corner_table_->Vertex(corner_table_->Next(corner)));
}
~~~~~
{:.draco-syntax }


### IsTopologySplit()

~~~~~
IsTopologySplit(encoder_symbol_id, *out_face_edge,
                         *out_encoder_split_symbol_id) {
  if (topology_split_data_.size() == 0)
    return false;
  if (topology_split_data_.back().source_symbol_id != encoder_symbol_id)
    return false;
  *out_face_edge = topology_split_data_.back().source_edge;
  *out_encoder_split_symbol_id =
          topology_split_data_.back().split_symbol_id;
  topology_split_data_.pop_back();
  return true;
}
~~~~~
{:.draco-syntax }


### DecodeAttributeConnectivitiesOnFace()

~~~~~
DecodeAttributeConnectivitiesOnFace(corner) {
  corners[3] = {corner, corner_table_->Next(corner),
                       corner_table_->Previous(corner)}
  for (c = 0; c < 3; ++c) {
    opp_corner = corner_table_->Opposite(corners[c]);
    if (opp_corner < 0) {
      for (uint32_t i = 0; i < attribute_data_.size(); ++i) {
        attribute_data_[i].attribute_seam_corners.push_back(corners[c]);
      }
      continue
    }
    for (uint32_t i = 0; i < attribute_data_.size(); ++i) {
      bool is_seam = traversal_decoder_.DecodeAttributeSeam(i);
      if (is_seam) {
        attribute_data_[i].attribute_seam_corners.push_back(corners[c]);
      }
    }
  }
}
~~~~~
{:.draco-syntax }


### SetOppositeCorners()

~~~~~
SetOppositeCorners(corner_0, corner_1) {
  corner_table_->SetOppositeCorner(corner_0, corner_1);
  corner_table_->SetOppositeCorner(corner_1, corner_0);
}
~~~~~
{:.draco-syntax }
