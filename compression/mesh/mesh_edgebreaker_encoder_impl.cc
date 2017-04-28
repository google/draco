// Copyright 2016 The Draco Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
#include "compression/mesh/mesh_edgebreaker_encoder_impl.h"

#include <algorithm>

#include "compression/attributes/mesh_attribute_indices_encoding_observer.h"
#include "compression/attributes/sequential_attribute_encoders_controller.h"
#include "compression/mesh/mesh_edgebreaker_encoder.h"
#include "compression/mesh/mesh_edgebreaker_traversal_predictive_encoder.h"
#include "compression/mesh/mesh_edgebreaker_traversal_valence_encoder.h"
#include "mesh/corner_table_iterators.h"
#include "mesh/corner_table_traversal_processor.h"
#include "mesh/edgebreaker_traverser.h"
#include "mesh/mesh_misc_functions.h"
#include "mesh/prediction_degree_traverser.h"

namespace draco {

typedef CornerIndex CornerIndex;
typedef FaceIndex FaceIndex;
typedef VertexIndex VertexIndex;

template <class TraversalEncoder>
MeshEdgeBreakerEncoderImpl<TraversalEncoder>::MeshEdgeBreakerEncoderImpl()
    : encoder_(nullptr),
      mesh_(nullptr),
      last_encoded_symbol_id_(-1),
      num_split_symbols_(0) {}

template <class TraversalEncoder>
bool MeshEdgeBreakerEncoderImpl<TraversalEncoder>::Init(
    MeshEdgeBreakerEncoder *encoder) {
  encoder_ = encoder;
  mesh_ = encoder->mesh();
  attribute_encoder_to_data_id_map_.clear();
  return true;
}

template <class TraversalEncoder>
const MeshAttributeCornerTable *
MeshEdgeBreakerEncoderImpl<TraversalEncoder>::GetAttributeCornerTable(
    int att_id) const {
  for (uint32_t i = 0; i < attribute_data_.size(); ++i) {
    if (attribute_data_[i].attribute_index == att_id) {
      if (attribute_data_[i].is_connectivity_used)
        return &attribute_data_[i].connectivity_data;
      return nullptr;
    }
  }
  return nullptr;
}

template <class TraversalEncoder>
const MeshAttributeIndicesEncodingData *
MeshEdgeBreakerEncoderImpl<TraversalEncoder>::GetAttributeEncodingData(
    int att_id) const {
  for (uint32_t i = 0; i < attribute_data_.size(); ++i) {
    if (attribute_data_[i].attribute_index == att_id)
      return &attribute_data_[i].encoding_data;
  }
  return &pos_encoding_data_;
}

template <class TraversalEncoder>
template <class TraverserT>
std::unique_ptr<PointsSequencer>
MeshEdgeBreakerEncoderImpl<TraversalEncoder>::CreateVertexTraversalSequencer(
    MeshAttributeIndicesEncodingData *encoding_data) {
  typedef typename TraverserT::TraversalObserver AttObserver;
  typedef typename TraverserT::TraversalProcessor AttProcessor;

  std::unique_ptr<MeshTraversalSequencer<TraverserT>> traversal_sequencer(
      new MeshTraversalSequencer<TraverserT>(mesh_, encoding_data));

  AttProcessor att_processor;
  AttObserver att_observer(corner_table_.get(), mesh_,
                           traversal_sequencer.get(), encoding_data);
  TraverserT att_traverser;

  att_processor.ResetProcessor(corner_table_.get());
  att_traverser.Init(std::move(att_processor), att_observer);

  traversal_sequencer->SetCornerOrder(processed_connectivity_corners_);
  traversal_sequencer->SetTraverser(att_traverser);
  return std::move(traversal_sequencer);
}

template <class TraversalEncoder>
bool MeshEdgeBreakerEncoderImpl<TraversalEncoder>::GenerateAttributesEncoder(
    int32_t att_id) {
  // For now, create one encoder for each attribute. Ideally we can share
  // the same encoder for attributes with the same connectivity (this is
  // especially true for per-vertex attributes).
  const int32_t element_type =
      GetEncoder()->mesh()->GetAttributeElementType(att_id);
  const PointAttribute *const att =
      GetEncoder()->point_cloud()->attribute(att_id);
  int32_t att_data_id = -1;
  for (uint32_t i = 0; i < attribute_data_.size(); ++i) {
    if (attribute_data_[i].attribute_index == att_id) {
      att_data_id = i;
      break;
    }
  }
  MeshTraversalMethod traversal_method = MESH_TRAVERSAL_DEPTH_FIRST;
  std::unique_ptr<PointsSequencer> sequencer;
  if (att->attribute_type() == GeometryAttribute::POSITION ||
      element_type == MESH_VERTEX_ATTRIBUTE ||
      (element_type == MESH_CORNER_ATTRIBUTE &&
       attribute_data_[att_data_id].connectivity_data.no_interior_seams())) {
    // Per-vertex attribute reached, use the basic corner table to traverse the
    // mesh.
    typedef CornerTableTraversalProcessor<CornerTable> AttProcessor;
    typedef MeshAttributeIndicesEncodingObserver<CornerTable> AttObserver;

    MeshAttributeIndicesEncodingData *encoding_data;
    if (att->attribute_type() == GeometryAttribute::POSITION) {
      encoding_data = &pos_encoding_data_;
    } else {
      encoding_data = &attribute_data_[att_data_id].encoding_data;
      attribute_data_[att_data_id].is_connectivity_used = false;
    }

    if (att->attribute_type() == GeometryAttribute::POSITION &&
        GetEncoder()->options()->GetSpeed() == 0) {
      // Traverser that is used to generate the encoding order of each
      // attribute.
      typedef PredictionDegreeTraverser<AttProcessor, AttObserver> AttTraverser;
      sequencer = CreateVertexTraversalSequencer<AttTraverser>(encoding_data);
      traversal_method = MESH_TRAVERSAL_PREDICTION_DEGREE;
    } else {
      // Traverser that is used to generate the encoding order of each
      // attribute.
      typedef EdgeBreakerTraverser<AttProcessor, AttObserver> AttTraverser;
      sequencer = CreateVertexTraversalSequencer<AttTraverser>(encoding_data);
    }
  } else {
    // Else use a general per-corner encoder.
    typedef CornerTableTraversalProcessor<MeshAttributeCornerTable>
        AttProcessor;
    typedef MeshAttributeIndicesEncodingObserver<MeshAttributeCornerTable>
        AttObserver;
    // Traverser that is used to generate the encoding order of each attribute.
    typedef EdgeBreakerTraverser<AttProcessor, AttObserver> AttTraverser;

    std::unique_ptr<MeshTraversalSequencer<AttTraverser>> traversal_sequencer(
        new MeshTraversalSequencer<AttTraverser>(
            mesh_, &attribute_data_[att_data_id].encoding_data));

    AttProcessor att_processor;
    AttObserver att_observer(&attribute_data_[att_data_id].connectivity_data,
                             mesh_, traversal_sequencer.get(),
                             &attribute_data_[att_data_id].encoding_data);
    AttTraverser att_traverser;

    att_processor.ResetProcessor(
        &attribute_data_[att_data_id].connectivity_data);
    att_traverser.Init(att_processor, att_observer);

    traversal_sequencer->SetCornerOrder(processed_connectivity_corners_);
    traversal_sequencer->SetTraverser(att_traverser);
    sequencer = std::move(traversal_sequencer);
  }

  if (!sequencer)
    return false;

  if (att_data_id == -1) {
    pos_traversal_method_ = traversal_method;
  } else {
    attribute_data_[att_data_id].traversal_method = traversal_method;
  }

  std::unique_ptr<SequentialAttributeEncodersController> att_controller(
      new SequentialAttributeEncodersController(std::move(sequencer), att_id));

  // Update the mapping between the encoder id and the attribute data id.
  // This will be used by the decoder to select the appropriate attribute
  // decoder and the correct connectivity.
  attribute_encoder_to_data_id_map_.push_back(att_data_id);
  GetEncoder()->AddAttributesEncoder(std::move(att_controller));
  return true;
}

template <class TraversalEncoder>
bool MeshEdgeBreakerEncoderImpl<TraversalEncoder>::
    EncodeAttributesEncoderIdentifier(int32_t att_encoder_id) {
  const int8_t att_data_id = attribute_encoder_to_data_id_map_[att_encoder_id];
  encoder_->buffer()->Encode(att_data_id);

  // Also encode the type of the encoder that we used.
  int32_t element_type = MESH_VERTEX_ATTRIBUTE;
  MeshTraversalMethod traversal_method;
  if (att_data_id >= 0) {
    const int32_t att_id = attribute_data_[att_data_id].attribute_index;
    element_type = GetEncoder()->mesh()->GetAttributeElementType(att_id);
    traversal_method = attribute_data_[att_data_id].traversal_method;
  } else {
    traversal_method = pos_traversal_method_;
  }
  if (element_type == MESH_VERTEX_ATTRIBUTE ||
      (element_type == MESH_CORNER_ATTRIBUTE &&
       attribute_data_[att_data_id].connectivity_data.no_interior_seams())) {
    // Per-vertex encoder.
    encoder_->buffer()->Encode(static_cast<uint8_t>(MESH_VERTEX_ATTRIBUTE));
  } else {
    // Per-corner encoder.
    encoder_->buffer()->Encode(static_cast<uint8_t>(MESH_CORNER_ATTRIBUTE));
  }
  // Encode the mesh traversal method.
  encoder_->buffer()->Encode(static_cast<uint8_t>(traversal_method));
  return true;
}

template <class TraversalEncoder>
bool MeshEdgeBreakerEncoderImpl<TraversalEncoder>::EncodeConnectivity() {
  // To encode the mesh, we need face connectivity data stored in a corner
  // table. To compute the connectivity we must use indices associated with
  // POSITION attribute, because they define which edges can be connected
  // together.
  corner_table_ = CreateCornerTable(mesh_);
  if (corner_table_ == nullptr) {
    // Failed to construct the corner table.
    return false;
  }

  traversal_encoder_.Init(this);

  // Encode the number of new vertices that were added because of non-manifold
  // edges/vertices in the input mesh. We will encode the actual mapping between
  // new and original vertices later because their order (and ids) may change
  // during the encoding.
  const uint32_t num_new_vertices = corner_table_->NumNewVertices();
  encoder_->buffer()->Encode(num_new_vertices);

  // Also encode the total number of vertices that is going to be encoded.
  // This can be different from the mesh_->num_points() + num_new_vertices,
  // because some of the vertices of the input mesh can be ignored (e.g.
  // vertices on degenerated faces or isolated vertices not attached to any
  // face).
  const uint32_t num_vertices_to_be_encoded =
      corner_table_->num_vertices() - corner_table_->NumIsolatedVertices();
  encoder_->buffer()->Encode(num_vertices_to_be_encoded);

  const uint32_t num_faces =
      corner_table_->num_faces() - corner_table_->NumDegeneratedFaces();
  encoder_->buffer()->Encode(num_faces);

  // Reset encoder data that may have been initialized in previous runs.
  visited_faces_.assign(mesh_->num_faces(), false);
  pos_encoding_data_.vertex_to_encoded_attribute_value_index_map.assign(
      corner_table_->num_vertices(), -1);
  pos_encoding_data_.encoded_attribute_value_index_to_corner_map.clear();
  pos_encoding_data_.encoded_attribute_value_index_to_corner_map.reserve(
      corner_table_->num_faces() * 3);
  visited_vertex_ids_.assign(corner_table_->num_vertices(), false);
  vertex_traversal_length_.clear();
  last_encoded_symbol_id_ = -1;
  num_split_symbols_ = 0;
  topology_split_event_data_.clear();
  face_to_split_symbol_map_.clear();
  visited_holes_.clear();
  vertex_hole_id_.assign(corner_table_->num_vertices(), -1);
  hole_event_data_.clear();
  processed_connectivity_corners_.clear();
  processed_connectivity_corners_.reserve(corner_table_->num_faces());
  pos_encoding_data_.num_values = 0;

  if (!FindHoles())
    return false;

  if (!InitAttributeData())
    return false;

  const int8_t num_attribute_data = attribute_data_.size();
  encoder_->buffer()->Encode(num_attribute_data);

  const int num_corners = corner_table_->num_corners();

  traversal_encoder_.Start();

  std::vector<CornerIndex> init_face_connectivity_corners;
  // Traverse the surface starting from each unvisited corner.
  for (int c_id = 0; c_id < num_corners; ++c_id) {
    CornerIndex corner_index(c_id);
    const FaceIndex face_id = corner_table_->Face(corner_index);
    if (visited_faces_[face_id.value()])
      continue;  // Face has been already processed.
    if (corner_table_->IsDegenerated(face_id))
      continue;  // Ignore degenerated faces.

    CornerIndex start_corner;
    const bool interior_config =
        FindInitFaceConfiguration(face_id, &start_corner);
    traversal_encoder_.EncodeStartFaceConfiguration(interior_config);

    if (interior_config) {
      // Select the correct vertex on the face as the root.
      corner_index = start_corner;
      const VertexIndex vert_id = corner_table_->Vertex(corner_index);
      // Mark all vertices of a given face as visited.
      const VertexIndex next_vert_id =
          corner_table_->Vertex(corner_table_->Next(corner_index));
      const VertexIndex prev_vert_id =
          corner_table_->Vertex(corner_table_->Previous(corner_index));

      visited_vertex_ids_[vert_id.value()] = true;
      visited_vertex_ids_[next_vert_id.value()] = true;
      visited_vertex_ids_[prev_vert_id.value()] = true;
      // New traversal started. Initiate it's length with the first vertex.
      vertex_traversal_length_.push_back(1);

      // Mark the face as visited.
      visited_faces_[face_id.value()] = true;
      // Start compressing from the opposite face of the "next" corner. This way
      // the first encoded corner corresponds to the tip corner of the regular
      // edgebreaker traversal (essentially the initial face can be then viewed
      // as a TOPOLOGY_C face).
      init_face_connectivity_corners.push_back(
          corner_table_->Next(corner_index));
      const CornerIndex opp_id =
          corner_table_->Opposite(corner_table_->Next(corner_index));
      const FaceIndex opp_face_id = corner_table_->Face(opp_id);
      if (opp_face_id != kInvalidFaceIndex &&
          !visited_faces_[opp_face_id.value()]) {
        if (!EncodeConnectivityFromCorner(opp_id))
          return false;
      }
    } else {
      // Bounary configuration. We start on a boundary rather than on a face.
      // First encode the hole that's opposite to the start_corner.
      EncodeHole(corner_table_->Next(start_corner), true);
      // Start processing the face opposite to the boundary edge (the face
      // containing the start_corner).
      if (!EncodeConnectivityFromCorner(start_corner))
        return false;
    }
  }
  // Reverse the order of connectivity corners to match the order in which
  // they are going to be decoded.
  std::reverse(processed_connectivity_corners_.begin(),
               processed_connectivity_corners_.end());
  // Append the init face connectivity corners (which are processed in order by
  // the decoder after the regular corners.
  processed_connectivity_corners_.insert(processed_connectivity_corners_.end(),
                                         init_face_connectivity_corners.begin(),
                                         init_face_connectivity_corners.end());
  // Emcode connectivity for all non-position attributes.
  if (attribute_data_.size() > 0) {
    // Use the same order of corner that will be used by the decoder.
    for (CornerIndex ci : processed_connectivity_corners_) {
      EncodeAttributeConnectivitiesOnFace(ci);
    }
  }
  traversal_encoder_.Done();

  // Encode the number of symbols.
  encoder_->buffer()->Encode(
      static_cast<uint32_t>(traversal_encoder_.NumEncodedSymbols()));

  // Encode the number of split symbols.
  encoder_->buffer()->Encode(static_cast<uint32_t>(num_split_symbols_));

  // Append the traversal buffer.
  encoder_->buffer()->Encode(
      static_cast<uint32_t>(traversal_encoder_.buffer().size()));
  encoder_->buffer()->Encode(traversal_encoder_.buffer().data(),
                             traversal_encoder_.buffer().size());

  // Encode topology split events.
  uint32_t num_events = topology_split_event_data_.size();
  encoder_->buffer()->Encode(num_events);
  if (num_events > 0) {
    // Encode split symbols using delta and varint coding. Split edges are
    // encoded using direct bit coding.
    int last_source_symbol_id = 0;  // Used for delta coding.
    for (uint32_t i = 0; i < num_events; ++i) {
      const TopologySplitEventData &event_data = topology_split_event_data_[i];
      // Encode source symbol id as delta from the previous source symbol id.
      // Source symbol ids are always stored in increasing order so the delta is
      // going to be positive.
      EncodeVarint<uint32_t>(
          event_data.source_symbol_id - last_source_symbol_id,
          encoder_->buffer());
      // Encode split symbol id as delta from the current source symbol id.
      // Split symbol id is always smaller than source symbol id so the below
      // delta is going to be positive.
      EncodeVarint<uint32_t>(
          event_data.source_symbol_id - event_data.split_symbol_id,
          encoder_->buffer());
      last_source_symbol_id = event_data.source_symbol_id;
    }
    encoder_->buffer()->StartBitEncoding(num_events * 2, false);
    for (uint32_t i = 0; i < num_events; ++i) {
      const TopologySplitEventData &event_data = topology_split_event_data_[i];
      encoder_->buffer()->EncodeLeastSignificantBits32(
          2, event_data.source_edge | (event_data.split_edge << 1));
    }
    encoder_->buffer()->EndBitEncoding();
  }
  // Encode hole events data.
  num_events = hole_event_data_.size();
  encoder_->buffer()->Encode(num_events);
  if (num_events > 0) {
    // Encode hole symbol ids using delta and varint coding. The symbol ids are
    // always stored in increasing order so the deltas are going to be positive.
    int last_symbol_id = 0;
    for (uint32_t i = 0; i < num_events; ++i) {
      EncodeVarint<uint32_t>(hole_event_data_[i].symbol_id - last_symbol_id,
                             encoder_->buffer());
      last_symbol_id = hole_event_data_[i].symbol_id;
    }
  }
  return true;
}

template <class TraversalEncoder>
bool MeshEdgeBreakerEncoderImpl<TraversalEncoder>::FindInitFaceConfiguration(
    FaceIndex face_id, CornerIndex *out_corner) const {
  CornerIndex corner_index = CornerIndex(3 * face_id.value());
  for (int i = 0; i < 3; ++i) {
    if (corner_table_->Opposite(corner_index) == kInvalidCornerIndex) {
      // If there is a boundary edge, the configuration is exterior and return
      // the CornerIndex opposite to the first boundary edge.
      *out_corner = corner_index;
      return false;
    }
    if (vertex_hole_id_[corner_table_->Vertex(corner_index).value()] != -1) {
      // Boundary vertex found. Find the first boundary edge attached to the
      // point and return the corner opposite to it.
      CornerIndex right_corner = corner_index;
      while (right_corner >= 0) {
        corner_index = right_corner;
        right_corner = corner_table_->SwingRight(right_corner);
      }
      // |corner_index| now lies on a boundary edge and its previous corner is
      // guaranteed to be the opposite corner of the boundary edge.
      *out_corner = corner_table_->Previous(corner_index);
      return false;
    }
    corner_index = corner_table_->Next(corner_index);
  }
  // Else we have an interior configuration. Return the first corner id.
  *out_corner = corner_index;
  return true;
}

template <class TraversalEncoder>
bool MeshEdgeBreakerEncoderImpl<TraversalEncoder>::EncodeConnectivityFromCorner(
    CornerIndex corner_id) {
  corner_traversal_stack_.clear();
  corner_traversal_stack_.push_back(corner_id);
  const int num_faces = mesh_->num_faces();
  while (!corner_traversal_stack_.empty()) {
    // Currently processed corner.
    corner_id = corner_traversal_stack_.back();
    // Make sure the face hasn't been visited yet.
    if (corner_id < 0 ||
        visited_faces_[corner_table_->Face(corner_id).value()]) {
      // This face has been already traversed.
      corner_traversal_stack_.pop_back();
      continue;
    }
    int num_visited_faces = 0;
    while (num_visited_faces < num_faces) {
      // Mark the current face as visited.
      ++num_visited_faces;
      ++last_encoded_symbol_id_;

      const FaceIndex face_id = corner_table_->Face(corner_id);
      visited_faces_[face_id.value()] = true;
      processed_connectivity_corners_.push_back(corner_id);
      traversal_encoder_.NewCornerReached(corner_id);
      const VertexIndex vert_id = corner_table_->Vertex(corner_id);
      const bool on_boundary = (vertex_hole_id_[vert_id.value()] != -1);
      if (!IsVertexVisited(vert_id)) {
        // A new unvisited vertex has been reached. We need to store its
        // position difference using next,prev, and opposite vertices.
        visited_vertex_ids_[vert_id.value()] = true;
        if (!on_boundary) {
          // If the vertex is on boundary it must correspond to an unvisited
          // hole and it will be encoded with TOPOLOGY_S symbol later).
          traversal_encoder_.EncodeSymbol(TOPOLOGY_C);
          // Move to the right triangle.
          corner_id = GetRightCorner(corner_id);
          continue;
        }
      }
      // The current vertex has been already visited or it was on a boundary.
      // We need to determine whether we can visit any of it's neighboring
      // faces.
      const CornerIndex right_corner_id = GetRightCorner(corner_id);
      const CornerIndex left_corner_id = GetLeftCorner(corner_id);
      const FaceIndex right_face_id = corner_table_->Face(right_corner_id);
      const FaceIndex left_face_id = corner_table_->Face(left_corner_id);
      if (IsRightFaceVisited(corner_id)) {
        // Right face has been already visited.
        // Check whether there is a topology split event.
        if (right_face_id != kInvalidFaceIndex)
          CheckAndStoreTopologySplitEvent(last_encoded_symbol_id_,
                                          face_id.value(), RIGHT_FACE_EDGE,
                                          right_face_id.value());
        if (IsLeftFaceVisited(corner_id)) {
          // Both neighboring faces are visited. End reached.
          // Check whether there is a topology split event on the left face.
          if (left_face_id != kInvalidFaceIndex)
            CheckAndStoreTopologySplitEvent(last_encoded_symbol_id_,
                                            face_id.value(), LEFT_FACE_EDGE,
                                            left_face_id.value());
          traversal_encoder_.EncodeSymbol(TOPOLOGY_E);
          corner_traversal_stack_.pop_back();
          break;  // Break from the while (num_visited_faces < num_faces) loop.
        } else {
          traversal_encoder_.EncodeSymbol(TOPOLOGY_R);
          // Go to the left face.
          corner_id = left_corner_id;
        }
      } else {
        // Right face was not visited.
        if (IsLeftFaceVisited(corner_id)) {
          // Check whether there is a topology split event on the left face.
          if (left_face_id != kInvalidFaceIndex)
            CheckAndStoreTopologySplitEvent(last_encoded_symbol_id_,
                                            face_id.value(), LEFT_FACE_EDGE,
                                            left_face_id.value());
          traversal_encoder_.EncodeSymbol(TOPOLOGY_L);
          // Left face visited, go to the right one.
          corner_id = right_corner_id;
        } else {
          traversal_encoder_.EncodeSymbol(TOPOLOGY_S);
          ++num_split_symbols_;
          // Both neighboring faces are unvisited, we need to visit both of
          // them.
          if (on_boundary) {
            // The tip vertex is on a hole boundary. If the hole hasn't been
            // visited yet we need to encode it.
            const int hole_id = vertex_hole_id_[vert_id.value()];
            if (!visited_holes_[hole_id]) {
              EncodeHole(corner_id, false);
              hole_event_data_.push_back(
                  HoleEventData(last_encoded_symbol_id_));
            }
          }
          face_to_split_symbol_map_[face_id.value()] = last_encoded_symbol_id_;
          // Split the traversal.
          // First make the top of the current corner stack point to the left
          // face (this one will be processed second).
          corner_traversal_stack_.back() = left_corner_id;
          // Add a new corner to the top of the stack (right face needs to
          // be traversed first).
          corner_traversal_stack_.push_back(right_corner_id);
          // Break from the while (num_visited_faces < num_faces) loop.
          break;
        }
      }
    }
  }
  return true;  // All corners have been processed.
}

template <class TraversalEncoder>
int MeshEdgeBreakerEncoderImpl<TraversalEncoder>::EncodeHole(
    CornerIndex start_corner_id, bool encode_first_vertex) {
  // We know that the start corner lies on a hole but we first need to find the
  // boundary edge going from that vertex. It is the first edge in CW
  // direction.
  CornerIndex corner_id = start_corner_id;
  corner_id = corner_table_->Previous(corner_id);
  while (corner_table_->Opposite(corner_id) != kInvalidCornerIndex) {
    corner_id = corner_table_->Opposite(corner_id);
    corner_id = corner_table_->Next(corner_id);
  }
  const VertexIndex start_vertex_id = corner_table_->Vertex(start_corner_id);

  int num_encoded_hole_verts = 0;
  if (encode_first_vertex) {
    visited_vertex_ids_[start_vertex_id.value()] = true;
    ++num_encoded_hole_verts;
  }

  // corner_id is now opposite to the boundary edge.
  // Mark the hole as visited.
  visited_holes_[vertex_hole_id_[start_vertex_id.value()]] = true;
  // Get the start vertex of the edge and use it as a reference.
  VertexIndex start_vert_id =
      corner_table_->Vertex(corner_table_->Next(corner_id));
  // Get the end vertex of the edge.
  VertexIndex act_vertex_id =
      corner_table_->Vertex(corner_table_->Previous(corner_id));
  while (act_vertex_id != start_vertex_id) {
    // Encode the end vertex of the boundary edge.

    start_vert_id = act_vertex_id;

    // Mark the vertex as visited.
    visited_vertex_ids_[act_vertex_id.value()] = true;
    ++num_encoded_hole_verts;
    corner_id = corner_table_->Next(corner_id);
    // Look for the next attached open boundary edge.
    while (corner_table_->Opposite(corner_id) != kInvalidCornerIndex) {
      corner_id = corner_table_->Opposite(corner_id);
      corner_id = corner_table_->Next(corner_id);
    }
    act_vertex_id = corner_table_->Vertex(corner_table_->Previous(corner_id));
  }
  return num_encoded_hole_verts;
}

template <class TraversalEncoder>
CornerIndex MeshEdgeBreakerEncoderImpl<TraversalEncoder>::GetRightCorner(
    CornerIndex corner_id) const {
  const CornerIndex next_corner_id = corner_table_->Next(corner_id);
  return corner_table_->Opposite(next_corner_id);
}

template <class TraversalEncoder>
CornerIndex MeshEdgeBreakerEncoderImpl<TraversalEncoder>::GetLeftCorner(
    CornerIndex corner_id) const {
  const CornerIndex prev_corner_id = corner_table_->Previous(corner_id);
  return corner_table_->Opposite(prev_corner_id);
}

template <class TraversalEncoder>
bool MeshEdgeBreakerEncoderImpl<TraversalEncoder>::IsRightFaceVisited(
    CornerIndex corner_id) const {
  const CornerIndex next_corner_id = corner_table_->Next(corner_id);
  const CornerIndex opp_corner_id = corner_table_->Opposite(next_corner_id);
  if (opp_corner_id != kInvalidCornerIndex)
    return visited_faces_[corner_table_->Face(opp_corner_id).value()];
  // Else we are on a boundary.
  return true;
}

template <class TraversalEncoder>
bool MeshEdgeBreakerEncoderImpl<TraversalEncoder>::IsLeftFaceVisited(
    CornerIndex corner_id) const {
  const CornerIndex prev_corner_id = corner_table_->Previous(corner_id);
  const CornerIndex opp_corner_id = corner_table_->Opposite(prev_corner_id);
  if (opp_corner_id != kInvalidCornerIndex)
    return visited_faces_[corner_table_->Face(opp_corner_id).value()];
  // Else we are on a boundary.
  return true;
}

template <class TraversalEncoder>
bool MeshEdgeBreakerEncoderImpl<TraversalEncoder>::FindHoles() {
  // TODO(ostava): Add more error checking for invalid geometry data.
  const int num_corners = corner_table_->num_corners();
  // Go over all corners and detect non-visited open boundaries
  for (CornerIndex i(0); i < num_corners; ++i) {
    if (corner_table_->IsDegenerated(corner_table_->Face(i)))
      continue;  // Don't process corners assigned to degenerated faces.
    if (corner_table_->Opposite(i) == kInvalidCornerIndex) {
      // No opposite corner means no opposite face, so the opposite edge
      // of the corner is an open boundary.
      // Check whether we have already traversed the boundary.
      VertexIndex boundary_vert_id =
          corner_table_->Vertex(corner_table_->Next(i));
      if (vertex_hole_id_[boundary_vert_id.value()] != -1) {
        // The start vertex of the boundary edge is already assigned to an
        // open boundary. No need to traverse it again.
        continue;
      }
      // Else we found a new open boundary and we are going to traverse along it
      // and mark all visited vertices.
      const int boundary_id = visited_holes_.size();
      visited_holes_.push_back(false);

      CornerIndex corner_id = i;
      while (vertex_hole_id_[boundary_vert_id.value()] == -1) {
        // Mark the first vertex on the open boundary.
        vertex_hole_id_[boundary_vert_id.value()] = boundary_id;
        corner_id = corner_table_->Next(corner_id);
        // Look for the next attached open boundary edge.
        while (corner_table_->Opposite(corner_id) != kInvalidCornerIndex) {
          corner_id = corner_table_->Opposite(corner_id);
          corner_id = corner_table_->Next(corner_id);
        }
        // Id of the next vertex in the vertex on the hole.
        boundary_vert_id =
            corner_table_->Vertex(corner_table_->Next(corner_id));
      }
    }
  }
  return true;
}

template <class TraversalEncoder>
int MeshEdgeBreakerEncoderImpl<TraversalEncoder>::GetSplitSymbolIdOnFace(
    int face_id) const {
  auto it = face_to_split_symbol_map_.find(face_id);
  if (it == face_to_split_symbol_map_.end())
    return -1;
  return it->second;
}

template <class TraversalEncoder>
void MeshEdgeBreakerEncoderImpl<
    TraversalEncoder>::CheckAndStoreTopologySplitEvent(int src_symbol_id,
                                                       int /* src_face_id */,
                                                       EdgeFaceName src_edge,
                                                       int neighbor_face_id) {
  const int symbol_id = GetSplitSymbolIdOnFace(neighbor_face_id);
  if (symbol_id == -1)
    return;  // Not a split symbol, no topology split event could happen.
  TopologySplitEventData event_data;

  event_data.split_symbol_id = symbol_id;
  // It's always the left edge for true split symbols (as the right edge is
  // traversed first).
  event_data.split_edge = LEFT_FACE_EDGE;

  event_data.source_symbol_id = src_symbol_id;
  event_data.source_edge = src_edge;
  topology_split_event_data_.push_back(event_data);
}

template <class TraversalEncoder>
bool MeshEdgeBreakerEncoderImpl<TraversalEncoder>::InitAttributeData() {
  const int num_attributes = mesh_->num_attributes();
  // Ignore the position attribute. It's decoded separately.
  attribute_data_.resize(num_attributes - 1);
  if (num_attributes == 1)
    return true;
  int data_index = 0;
  for (int i = 0; i < num_attributes; ++i) {
    const int32_t att_index = i;
    if (mesh_->attribute(att_index)->attribute_type() ==
        GeometryAttribute::POSITION)
      continue;
    const PointAttribute *const att = mesh_->attribute(att_index);
    attribute_data_[data_index].attribute_index = att_index;
    attribute_data_[data_index]
        .encoding_data.encoded_attribute_value_index_to_corner_map.clear();
    attribute_data_[data_index]
        .encoding_data.encoded_attribute_value_index_to_corner_map.reserve(
            corner_table_->num_corners());
    attribute_data_[data_index]
        .encoding_data.vertex_to_encoded_attribute_value_index_map.assign(
            corner_table_->num_corners(), -1);
    attribute_data_[data_index].encoding_data.num_values = 0;
    attribute_data_[data_index].connectivity_data.InitFromAttribute(
        mesh_, corner_table_.get(), att);
    ++data_index;
  }
  return true;
}

// TODO(ostava): Note that if the input mesh used the same attribute index on
// multiple different vertices, such attribute will be duplicated using the
// encoding below. Eventually, we may consider either using a different encoding
// scheme for such cases, or at least deduplicating the attributes in the
// decoder.
template <class TraversalEncoder>
bool MeshEdgeBreakerEncoderImpl<
    TraversalEncoder>::EncodeAttributeConnectivitiesOnFace(CornerIndex corner) {
  // Three corners of the face.
  const CornerIndex corners[3] = {corner, corner_table_->Next(corner),
                                  corner_table_->Previous(corner)};

  for (int c = 0; c < 3; ++c) {
    const CornerIndex opp_corner = corner_table_->Opposite(corners[c]);
    if (opp_corner < 0)
      continue;  // Don't encode attribute seams on boundary edges.

    for (uint32_t i = 0; i < attribute_data_.size(); ++i) {
      if (attribute_data_[i].connectivity_data.IsCornerOppositeToSeamEdge(
              corners[c])) {
        traversal_encoder_.EncodeAttributeSeam(i, true);
      } else {
        traversal_encoder_.EncodeAttributeSeam(i, false);
      }
    }
  }
  return true;
}

template class MeshEdgeBreakerEncoderImpl<MeshEdgeBreakerTraversalEncoder>;
template class MeshEdgeBreakerEncoderImpl<
    MeshEdgeBreakerTraversalPredictiveEncoder>;
template class MeshEdgeBreakerEncoderImpl<
    MeshEdgeBreakerTraversalValenceEncoder>;

}  // namespace draco
