## EdgeBreaker Decoder

### InitializeDecoder()

<div class="syntax">
InitializeDecoder() {                                                 <b>Type</b>
  <b>edgebreaker_decoder_type</b>                                            UI8
}

</div>


### DecodeConnectivity()

<div class="syntax">
DecodeConnectivity() {                                                <b>Type</b>
  <b>num_new_verts</b>                                                       UI32
  <b>num_encoded_vertices</b>                                                UI32
  <b>num_faces</b>                                                           UI32
  <b>num_attribute_data</b>                                                  I8
  <b>num_encoded_symbols</b>                                                 UI32
  <b>num_encoded_split_symbols</b>                                           UI32
  <b>encoded_connectivity_size</b>                                           UI32
  // file pointer must be set to current position + encoded_connectivity_size
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

</div>


### AssignPointsToCorners()

<div class="syntax">
AssignPointsToCorners() {                                             <b>Type</b>
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

</div>


### DecodeConnectivity()

<div class="syntax">
DecodeConnectivity(num_symbols) {                                     <b>Type</b>
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

</div>


### UpdateCornerTableForSymbolC()

<div class="syntax">
UpdateCornerTableForSymbolC(corner) {                                 <b>Type</b>
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

</div>



### UpdateCornerTableForSymbolLR()

<div class="syntax">
UpdateCornerTableForSymbolLR(corner, symbol) {                        <b>Type</b>
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

</div>


### HandleSymbolS()

<div class="syntax">
HandleSymbolS(corner) {                                               <b>Type</b>
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

</div>


### UpdateCornerTableForSymbolE()

<div class="syntax">
UpdateCornerTableForSymbolE() {                                        <b>Type</b>
  corner_table_->MapCornerToVertex(corner, num_vertices++);
  corner_table_->MapCornerToVertex(corner + 1, num_vertices++);
  corner_table_->MapCornerToVertex(corner + 2, num_vertices++);
}

</div>


### UpdateCornerTableForInteriorFace()

<div class="syntax">
UpdateCornerTableForInteriorFace() {                                  <b>Type</b>
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

</div>


### IsTopologySplit()

<div class="syntax">
IsTopologySplit(encoder_symbol_id, *out_face_edge,                    <b>Type</b>

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

</div>


### DecodeAttributeConnectivitiesOnFace()

<div class="syntax">
DecodeAttributeConnectivitiesOnFace(corner) {                         <b>Type</b>
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

</div>


### SetOppositeCorners()

<div class="syntax">
SetOppositeCorners(corner_0, corner_1) {                              <b>Type</b>
  corner_table_->SetOppositeCorner(corner_0, corner_1);
  corner_table_->SetOppositeCorner(corner_1, corner_0);
}

</div>


## EdgeBreaker Hole and Topology Split Events

### DecodeHoleAndTopologySplitEvents()

FIXME: Escaping angle brackets

<div class="syntax">
DecodeHoleAndTopologySplitEvents() {                                  <b>Type</b>
  <b>num_topologoy_splits</b>                                                UI32
  source_symbol_id = 0
  for (i = 0; i < num_topologoy_splits; ++i) {
    DecodeVarint\<UI32\>(&delta)
    split_data[i].source_symbol_id = delta + source_symbol_id
    DecodeVarint\<UI32\>(&delta)
    split_data[i].split_symbol_id = source_symbol_id - delta
  }
  for (i = 0; i < num_topologoy_splits; ++i) {
    <b>split_data[i].split_edge</b>                                          bits1
    <b>split_data[i].source_edge</b>                                         bits1
  }
  <b>num_hole_events</b>                                                     UI32
  symbol_id = 0
  for (i = 0; i < num_hole_events; ++i) {
    DecodeVarint\<UI32\>(&delta)
    hole_data[i].symbol_id = delta + symbol_id
  }
    return bytes_decoded;
}

</div>

### CreateAttributesDecoder

FIXME: Escaping angle brackets

<div class="syntax">
CreateAttributesDecoder() {                                           <b>Type</b>
  <b>att_data_id</b>                                                         I8
  <b>decoder_type</b>                                                        UI8
  if (att_data_id >= 0) {
    attribute_data_[att_data_id].decoder_id = att_decoder_id;
  }
  <b>traversal_method_encoded</b>                                            UI8
  if (decoder_type == MESH_VERTEX_ATTRIBUTE) {
    if (att_data_id < 0) {
      encoding_data = &pos_encoding_data_;
    } else {
      encoding_data = &attribute_data_[att_data_id].encoding_data;
      attribute_data_[att_data_id].is_connectivity_used = false;
    }
    if (traversal_method == MESH_TRAVERSAL_DEPTH_FIRST) {
      typedef EdgeBreakerTraverser\<AttProcessor, AttObserver\> AttTraverser;
      sequencer = CreateVertexTraversalSequencer\<AttTraverser\>(encoding_data);
    } else if (traversal_method == MESH_TRAVERSAL_PREDICTION_DEGREE) {
      typedef PredictionDegreeTraverser\<AttProcessor, AttObserver\> AttTraverser;
      sequencer = CreateVertexTraversalSequencer\<AttTraverser\>(encoding_data);
    }
  } else {
    // TODO
  }
  att_controller(new SequentialAttributeDecodersController(std::move(sequencer)))
  decoder_->SetAttributesDecoder(att_decoder_id, std::move(att_controller));
}

</div>


## Edgebreaker Traversal Decoder

### EdgebreakerTraversal_Start()

<div class="syntax">
EdgebreakerTraversal_Start() {                                        <b>Type</b>
  <b>size</b>                                                                UI64
  <b>symbol_buffer_</b>                                                      size * UI8
  <b>size</b>                                                                UI64
  <b>start_face_buffer_</b>                                                  size * UI8
  if (num_attribute_data_ > 0) {
    attribute_connectivity_decoders_ = std::unique_ptr<BinaryDecoder[]>(
          new BinaryDecoder[num_attribute_data_]);
    for (i = 0; i < num_attribute_data_; ++i) {
      attribute_connectivity_decoders_[i].StartDecoding()
      // RansBitDecoder_StartDecoding
  }
}

</div>


### Traversal_DecodeSymbol()

~~~~~
Traversal_DecodeSymbol() {
  symbol_buffer_.DecodeLeastSignificantBits32(1, &symbol);                   bits1
  if (symbol != TOPOLOGY_C) {
    symbol_buffer_.DecodeLeastSignificantBits32(2, &symbol_suffix);          bits2
    symbol |= (symbol_suffix << 1);
  }
  return symbol
}
~~~~~


### DecodeAttributeSeam()

~~~~~
DecodeAttributeSeam(int attribute) {
  return attribute_connectivity_decoders_[attribute].DecodeNextBit();
}
~~~~~


## EdgeBreaker Traversal Valence Decoder

### EdgeBreakerTraversalValence_Start()

~~~~~
EdgeBreakerTraversalValence_Start(num_vertices, num_attribute_data) {
  out_buffer = EdgebreakerTraversal_Start()
  num_split_symbols                                                          I32
  mode == 0                                                                  I8
  num_vertices_ += num_split_symbols
  vertex_valences_ init to 0
  vertex_valences_.resize(num_vertices_, 0);
  min_valence_ = 2;
  max_valence_ = 7;
  num_unique_valences = 6 (max_valence_ - min_valence_ + 1)
  for (i = 0; i < num_unique_valences; ++i) {
    DecodeVarint<UI32>(&num_symbols, out_buffer)
    If (num_symbols > 0) {
      DecodeSymbols(num_symbols, out_buffer, &context_symbols_[i])
    }
    context_counters_[i] = num_symbols
  }
  return out_buffer;
}
~~~~~



### TraversalValence_DecodeSymbol()

~~~~~
TraversalValence_DecodeSymbol() {
  if (active_context_ != -1) {
    symbol_id  = context_symbols_[active_context_]
                                                      [--context_counters_[active_context_]]
    last_symbol_ = edge_breaker_symbol_to_topology_id[symbol_id]
  } else {
    last_symbol_ = Traversal_DecodeSymbol()
  }
  return last_symbol_
}
~~~~~



### TraversalValence_NewActiveCornerReached()

~~~~~
TraversalValence_NewActiveCornerReached(corner) {
  switch (last_symbol_) {
    case TOPOLOGY_C:
    case TOPOLOGY_S:
      vertex_valences_[ct(next)] += 1;
      vertex_valences_[ct(prev)] += 1;
      break;
    case TOPOLOGY_R:
      vertex_valences_[corner] += 1;
      vertex_valences_[ct(next)] += 1;
      vertex_valences_[ct(prev)] += 2;
      break;
    case TOPOLOGY_L:
      vertex_valences_[corner] += 1;
      vertex_valences_[ct(next)] += 2;
      vertex_valences_[ct(prev)] += 1;
      break;
    case TOPOLOGY_E:
      vertex_valences_[corner] += 2;
      vertex_valences_[ct(next)] += 2;
      vertex_valences_[ct(prev)] += 2;
      break;
  }
  valence = vertex_valences_[ct(next)]
  valence = max(valence, min_valence_)
  valence = min(valence, max_valence_)
  active_context_ = (valence - min_valence_);
}
~~~~~



### TraversalValence_MergeVertices()

~~~~~
TraversalValence_MergeVertices(dest, source) {
  vertex_valences_[dest] += vertex_valences_[source];
}
~~~~~


## Attributes Decoder

### DecodeAttributesDecoderData()

~~~~~
DecodeAttributesDecoderData(buffer) {
  num_attributes                                                             I32
  point_attribute_ids_.resize(num_attributes);
  for (i = 0; i < num_attributes; ++i) {
    att_type                                                                 UI8
    data_type                                                                UI8
    components_count                                                         UI8
    normalized                                                               UI8
    custom_id                                                                UI16
    Initialize GeometryAttribute ga
    att_id = pc->AddAttribute(new PointAttribute(ga));
    point_attribute_ids_[i] = att_id;
}
~~~~~



## Sequential Attributes Decoders Controller

### DecodeAttributesDecoderData()

~~~~~
DecodeAttributesDecoderData(buffer) {
  AttributesDecoder_DecodeAttributesDecoderData(buffer)
  sequential_decoders_.resize(num_attributes());
  for (i = 0; i < num_attributes(); ++i) {
    decoder_type                                                             UI8
    sequential_decoders_[i] = CreateSequentialDecoder(decoder_type);
    sequential_decoders_[i]->Initialize(decoder(), GetAttributeId(i))
}
~~~~~


### DecodeAttributes()

~~~~~
DecodeAttributes(buffer) {
  sequencer_->GenerateSequence(&point_ids_)
  for (i = 0; i < num_attributes(); ++i) {
    pa = decoder()->point_cloud()->attribute(GetAttributeId(i));
    sequencer_->UpdatePointToAttributeIndexMapping(pa)
  }
  for (i = 0; i < num_attributes(); ++i) {
    sequential_decoders_[i]->Decode(point_ids_, buffer)
    //SequentialAttributeDecoder_Decode()
  }
}
~~~~~



### CreateSequentialDecoder()

~~~~~
CreateSequentialDecoder(type) {
  switch (type) {
    case SEQUENTIAL_ATTRIBUTE_ENCODER_GENERIC:
      return new SequentialAttributeDecoder()
    case SEQUENTIAL_ATTRIBUTE_ENCODER_INTEGER:
      return new SequentialIntegerAttributeDecoder()
    case SEQUENTIAL_ATTRIBUTE_ENCODER_QUANTIZATION:
      return new SequentialQuantizationAttributeDecoder()
    case SEQUENTIAL_ATTRIBUTE_ENCODER_NORMALS:
      return new SequentialNormalAttributeDecoder()
  }
}
~~~~~


## Sequential Attribute Decoder

~~~~~
Initialize(...) {
  // Init some members
}
~~~~~


### DecodeValues()

~~~~~
DecodeValues(const std::vector<PointIndex> &point_ids) {
  num_values = point_ids.size();
  entry_size = attribute_->byte_stride();
  std::unique_ptr<uint8_t[]> value_data_ptr(new uint8_t[entry_size]);
  out_byte_pos = 0;
  for (i = 0; i < num_values; ++i) {
   value_data                                                         UI8 * entry_size
    attribute_->buffer()->Write(out_byte_pos, value_data, entry_size);
    out_byte_pos += entry_size;
  }
}
~~~~~


## Sequential Integer Attribute Decoder

~~~~~
Initialize(...) {
  SequentialAttributeDecoder_Initialize()
}
~~~~~


### DecodeValues()

~~~~~
DecodeValues(point_ids) {
  prediction_scheme_method                                                   I8
  if (prediction_scheme_method != PREDICTION_NONE) {
    prediction_transform_type                                                I8
    prediction_scheme_ = CreateIntPredictionScheme(...)
  }
  if (prediction_scheme_) {
  }
  DecodeIntegerValues(point_ids)
  //SequentialQuantizationAttributeDecoder_DecodeIntegerValues()
  //StoreValues()
  DequantizeValues(num_values)
}
~~~~~


### DecodeIntegerValues()

~~~~~
DecodeIntegerValues(point_ids) {
  compressed                                                                 UI8
  if (compressed) {
    DecodeSymbols(..., values_.data())
  } else {
  // TODO
  }
  if (!prediction_scheme_->AreCorrectionsPositive()) {
    ConvertSymbolsToSignedInts(...)
  }
  if (prediction_scheme_) {
    prediction_scheme_->DecodePredictionData(buffer)
    // DecodeTransformData(buffer)
    if (!values_.empty()) {
      prediction_scheme_->Decode(values_.data(), &values_[0],
                                      values_.size(), num_components, point_ids.data())
      // MeshPredictionSchemeParallelogram_Decode()
}
~~~~~



## Sequential Quantization Attribute Decoder

~~~~~
Initialize(...) {
  SequentialIntegerAttributeDecoder_Initialize()
}
~~~~~


### DecodeIntegerValues()

~~~~~
DecodeIntegerValues(point_ids) {
  // DecodeQuantizedDataInfo()
  num_components = attribute()->components_count();
  for (i = 0; i < num_components; ++i) {
    min_value_[i]                                                            F32
  }
  max_value_dif_                                                             F32
  quantization_bits_                                                         UI8
  SequentialIntegerAttributeDecoder::DecodeIntegerValues()
}
~~~~~


### DequantizeValues()

~~~~~
DequantizeValues(num_values) {
  max_quantized_value = (1 << (quantization_bits_)) - 1;
  num_components = attribute()->components_count();
  entry_size = sizeof(float) * num_components;
  quant_val_id = 0;
  out_byte_pos = 0;
  for (i = 0; i < num_values; ++i) {
    for (c = 0; c < num_components; ++c) {
      value = dequantizer.DequantizeFloat(values()->at(quant_val_id++));
      value = value + min_value_[c];
      att_val[c] = value;
    }
    attribute()->buffer()->Write(out_byte_pos, att_val.get(), entry_size);
    out_byte_pos += entry_size;
  }
}
~~~~~



## Prediction Scheme Transform

### ComputeOriginalValue()

~~~~~
ComputeOriginalValue(const DataTypeT *predicted_vals,
                                   const CorrTypeT *corr_vals,
                                   DataTypeT *out_original_vals, int val_id) {
  for (i = 0; i < num_components_; ++i) {
    out_original_vals[i] = predicted_vals[i] + corr_vals[val_id + i];
  }
}
~~~~~



## Prediction Scheme Wrap Transform

### DecodeTransformData()

~~~~~
DecodeTransformData(buffer) {
  min_value_                                                                 DT
  max_value_                                                                 DT
}
~~~~~


### ComputeOriginalValue()

~~~~~
ComputeOriginalValue(const DataTypeT *predicted_vals,
                                   const CorrTypeT *corr_vals,
                                   DataTypeT *out_original_vals, int val_id) {
  clamped_vals = ClampPredictedValue(predicted_vals);
  ComputeOriginalValue(clamped_vals, corr_vals, out_original_vals, val_id)
  // PredictionSchemeTransform_ComputeOriginalValue()
  for (i = 0; i < this->num_components(); ++i) {
    if (out_original_vals[i] > max_value_) {
      out_original_vals[i] -= max_dif_;
    } else if (out_original_vals[i] < min_value_) {
      out_original_vals[i] += max_dif_;
    }
}
~~~~~


### ClampPredictedValue()

~~~~~
ClampPredictedValue(const DataTypeT *predicted_val) {
  for (i = 0; i < this->num_components(); ++i) {
    clamped_value_[i] = min(predicted_val[i], max_value_)
    clamped_value_[i] = max(predicted_val[i], min_value_)
  }
  return &clamped_value_[0];
}
~~~~~



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


MeshPredictionSchemeParallelogramShared

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


### ProcessCorner()

~~~~~
ProcessCorner(corner_id) {
  traverser_.TraverseFromCorner(corner_id);
}
~~~~~


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


PointsSequencer

### AddPointId()

~~~~~
AddPointId(point_id) {
  out_point_ids_->push_back(point_id);
}
~~~~~



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



## Mesh Attribute Corner Table

### bool IsCornerOnSeam()

~~~~~
bool IsCornerOnSeam(corner) {
  return is_vertex_on_seam_[corner_table_->Vertex(corner)];
}
~~~~~


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


### MarkSeam()

~~~~~
MarkSeam(c) {
  is_edge_on_seam_[c] = true;
  is_vertex_on_seam_[corner_table_->Vertex(corner_table_->Next(c))] = true;
  is_vertex_on_seam_[corner_table_->Vertex(corner_table_->Previous(c))
                         ] = true;
}
~~~~~


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




## Symbol Decoding

### DecodeSymbols()

~~~~~
DecodeSymbols(num_symbols, out_buffer, out_values) {
  scheme                                                                     UI8
  If (scheme == 0) {
    DecodeTaggedSymbols<>(num_symbols, src_buffer, out_values)
  } else if (scheme == 1) {
    DecodeRawSymbols<>(num_symbols, src_buffer, out_values)
  }
}
~~~~~


### DecodeTaggedSymbols()

~~~~~
DecodeTaggedSymbols() {
  FIXME
}
~~~~~



### DecodeRawSymbols()

~~~~~
DecodeRawSymbols() {
  max_bit_length                                                             UI8
  DecodeRawSymbolsInternal(max_bit_length, out_values)
  return symbols
}
~~~~~



### DecodeRawSymbolsInternal()

~~~~~
DecodeRawSymbolsInternal(max_bit_length, out_values) {
  decoder = CreateRansSymbolDecoder(max_bit_length)
  decoder.StartDecoding()
  // RansSymbolDecoder_StartDecoding
  for (i = 0; i < num_values; ++i) {
    out_values[i] = decoder.DecodeSymbol()
    // RansSymbolDecoder_DecodeSymbol
  }
}
~~~~~


### CreateRansSymbolDecoder()

~~~~~
CreateRansSymbolDecoder(max_bit_length) {
  rans_precision_bits  = (3 * max_bit_length) / 2;
  rans_precision_bits = min(rans_precision_bits, 20)
  rans_precision_bits = max(rans_precision_bits, 12)
  rans_precision = 1 << rans_precision_bits_;
  l_rans_base = rans_precision * 4;
  num_symbols_                                                               UI32
  for (i = 0; i < num_symbols_; ++i) {
    prob_data                                                                UI8
    if ((prob_data & 3) == 3) {
      offset = prob_data >> 2
      for (j = 0; j < offset + 1; ++j) {
        probability_table_[i + j] = 0;
      }
      i += offset;
    } else {
      prob = prob_data >> 2
      for (j = 0; j < token; ++j) {
        eb                                                                   UI8
        prob = prob | (eb << (8 * (j + 1) - 2)
      }
      probability_table_[i] = prob;
    }
  }
  rans_build_look_up_table()
}
~~~~~


### RansSymbolDecoder_StartDecoding()

~~~~~
RansSymbolDecoder_StartDecoding() {
  bytes_encoded                                                              UI64
  buffer                                                                     bytes_encoded * UI8
  rans_read_init(buffer, bytes_encoded)
}
~~~~~



### RansSymbolDecoder_DecodeSymbol()

~~~~~
RansSymbolDecoder_DecodeSymbol() {
  ans_.rans_read()
}
~~~~~


## Rans Decoding

### ans_read_init()

~~~~~
ans_read_init(struct AnsDecoder *const ans, const uint8_t *const buf,
                       int offset) {
  x = buf[offset - 1] >> 6
  If (x == 0) {
    ans->buf_offset = offset - 1;
    ans->state = buf[offset - 1] & 0x3F;
  } else if (x == 1) {
    ans->buf_offset = offset - 2;
    ans->state = mem_get_le16(buf + offset - 2) & 0x3FFF;
  } else if (x == 2) {
    ans->buf_offset = offset - 3;
    ans->state = mem_get_le24(buf + offset - 3) & 0x3FFFFF;
  } else if (x == 3) {
   // x == 3 implies this byte is a superframe marker
    return 1;
  }
  ans->state += l_base;
}
~~~~~



### int rabs_desc_read()

~~~~~
int rabs_desc_read(struct AnsDecoder *ans, AnsP8 p0) {
  AnsP8 p = ans_p8_precision - p0;
  if (ans->state < l_base) {
    ans->state = ans->state * io_base + ans->buf[--ans->buf_offset];
  }
  x = ans->state;
  quot = x / ans_p8_precision;
  rem = x % ans_p8_precision;
  xn = quot * p;
  val = rem < p;
  if (val) {
    ans->state = xn + rem;
  } else {
    ans->state = x - xn - p;
  }
  return val;
}
~~~~~



### rans_read_init()

~~~~~
rans_read_init(UI8 *buf, int offset) {
  ans_.buf = buf;
  x = buf[offset - 1] >> 6
  If (x == 0) {
    ans_.buf_offset = offset - 1;
    ans_.state = buf[offset - 1] & 0x3F;
  } else if (x == 1) {
    ans_.buf_offset = offset - 2;
    ans_.state = mem_get_le16(buf + offset - 2) & 0x3FFF;
  } else if (x == 2) {
    ans_.buf_offset = offset - 3;
    ans_.state = mem_get_le24(buf + offset - 3) & 0x3FFFFF;
  } else if (x == 3) {
    ans_.buf_offset = offset - 4;
    ans_.state = mem_get_le32(buf + offset - 4) & 0x3FFFFFFF;
  }
  ans_.state += l_rans_base;
}
~~~~~




### rans_build_look_up_table()

~~~~~
rans_build_look_up_table() {
  cum_prob = 0
  act_prob = 0
  for (i = 0; i < num_symbols; ++i) {
    probability_table_[i].prob = token_probs[i];
    probability_table_[i].cum_prob = cum_prob;
    cum_prob += token_probs[i];
    for (j = act_prob; j < cum_prob; ++j) {
      Lut_table_[j] = i
    }
    act_prob = cum_prob
}
~~~~~



### rans_read()

~~~~~
rans_read() {
  while (ans_.state < l_rans_base) {
    ans_.state = ans_.state * io_base + ans_.buf[--ans_.buf_offset];
  }
  quo = ans_.state / rans_precision;
  rem = ans_.state % rans_precision;
  sym = fetch_sym()
  ans_.state = quo * sym.prob + rem - sym.cum_prob;
  return sym.val;
}
~~~~~


### fetch_sym()

~~~~~
fetch_sym() {
  symbol = lut_table[rem]
  out->val = symbol
  out->prob = probability_table_[symbol].prob;
  out->cum_prob = probability_table_[symbol].cum_prob;
}
~~~~~



## Rans Bit Decoder

### RansBitDecoder_StartDecoding()

~~~~~
RansBitDecoder_StartDecoding(DecoderBuffer *source_buffer) {
  prob_zero_                                                                 UI8
  size                                                                       UI32
  buffer_                                                                    size * UI8
  ans_read_init(&ans_decoder_, buffer_, size)
}
~~~~~


### DecodeNextBit()

~~~~~
DecodeNextBit() {
  uint8_t bit = rabs_desc_read(&ans_decoder_, prob_zero_);
  return bit > 0;
}
~~~~~


## Core Functions

### DecodeVarint<IT>

~~~~~
DecodeVarint<IT>() {
  If (std::is_unsigned<IT>::value) {
    in                                                                       UI8
    If (in & (1 << 7)) {
      out = DecodeVarint<IT>()
      out = (out << 7) | (in & ((1 << 7) - 1))
    } else {
      typename std::make_unsigned<IT>::type UIT;
      out = DecodeVarint<UIT>()
      out = ConvertSymbolToSignedInt(out)
    }
    return out;
}
~~~~~



### ConvertSymbolToSignedInt()

~~~~~
ConvertSymbolToSignedInt() {
  abs_val = val >> 1
  If (val & 1 == 0) {
    return abs_val
  } else {
    signed_val = -abs_val - 1
  }
  return signed_val
}
~~~~~



Sequential Decoder

### decode_connectivity()

~~~~~
decode_connectivity() {
  num_faces                                                       I32
  num_points                                                      I32
  connectivity _method                                            UI8
  If (connectivity _method == 0) {
    // TODO
  } else {
    loop num_faces {
      If (num_points < 256) {
        face[]                                                    UI8
      } else if (num_points < (1 << 16)) {
        face[]                                                    UI16
      } else {
        face[]                                                    UI32
      }
    }
  }
}
~~~~~
