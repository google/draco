
## EdgeBreaker Traversal Valence Decoder

### EdgeBreakerTraversalValence_Start()

~~~~~
EdgeBreakerTraversalValence_Start(num_vertices, num_attribute_data) {
  out_buffer = EdgebreakerTraversal_Start()
  num_split_symbols                                                     I32
  mode == 0                                                             I8
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
{:.draco-syntax }



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
{:.draco-syntax }



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
{:.draco-syntax }


### TraversalValence_MergeVertices()

~~~~~
TraversalValence_MergeVertices(dest, source) {
  vertex_valences_[dest] += vertex_valences_[source];
}
~~~~~
{:.draco-syntax }
