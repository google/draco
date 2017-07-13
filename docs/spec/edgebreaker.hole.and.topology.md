
## EdgeBreaker Hole and Topology Split Events


### DecodeHoleAndTopologySplitEvents()

~~~~~
DecodeHoleAndTopologySplitEvents() {
  num_topologoy_splits                                                  UI32
  source_symbol_id = 0
  for (i = 0; i < num_topologoy_splits; ++i) {
    DecodeVarint<UI32>(&delta)
    split_data[i].source_symbol_id = delta + source_symbol_id
    DecodeVarint<UI32>(&delta)
    split_data[i].split_symbol_id = source_symbol_id - delta
  }
  for (i = 0; i < num_topologoy_splits; ++i) {
    split_data[i].split_edge                                            bits1
    split_data[i].source_edge                                           bits1
  }
  num_hole_events                                                       UI32
  symbol_id = 0
  for (i = 0; i < num_hole_events; ++i) {
    DecodeVarint<UI32>(&delta)
    hole_data[i].symbol_id = delta + symbol_id
  }
    return bytes_decoded;
}
~~~~~
{:.draco-syntax }


### CreateAttributesDecoder

~~~~~
CreateAttributesDecoder() {
  att_data_id                                                           I8
  decoder_type                                                          UI8
  if (att_data_id >= 0) {
    attribute_data_[att_data_id].decoder_id = att_decoder_id;
  }
  traversal_method_encoded                                              UI8
  if (decoder_type == MESH_VERTEX_ATTRIBUTE) {
    if (att_data_id < 0) {
      encoding_data = &pos_encoding_data_;
    } else {
      encoding_data = &attribute_data_[att_data_id].encoding_data;
      attribute_data_[att_data_id].is_connectivity_used = false;
    }
    if (traversal_method == MESH_TRAVERSAL_DEPTH_FIRST) {
      typedef EdgeBreakerTraverser<AttProcessor, AttObserver> AttTraverser;
      sequencer = CreateVertexTraversalSequencer<AttTraverser>(encoding_data);
    } else if (traversal_method == MESH_TRAVERSAL_PREDICTION_DEGREE) {
      typedef PredictionDegreeTraverser<AttProcessor, AttObserver> AttTraverser;
      sequencer = CreateVertexTraversalSequencer<AttTraverser>(encoding_data);
    }
  } else {
    // TODO
  }
  att_controller(new SequentialAttributeDecodersController(std::move(sequencer)))
  decoder_->SetAttributesDecoder(att_decoder_id, std::move(att_controller));
}
~~~~~
{:.draco-syntax }

