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
#ifndef DRACO_COMPRESSION_MESH_MESH_EDGEBREAKER_TRAVERSAL_DECODER_H_
#define DRACO_COMPRESSION_MESH_MESH_EDGEBREAKER_TRAVERSAL_DECODER_H_

#include "compression/mesh/mesh_edgebreaker_decoder.h"
#include "compression/mesh/mesh_edgebreaker_decoder_impl_interface.h"
#include "compression/mesh/mesh_edgebreaker_shared.h"
#include "core/rans_coding.h"

namespace draco {

typedef RAnsBitDecoder BinaryDecoder;

// Default implementation of the edgebreaker traversal decoder that reads the
// traversal data directly from a buffer.
class MeshEdgeBreakerTraversalDecoder {
 public:
  MeshEdgeBreakerTraversalDecoder()
      : attribute_connectivity_decoders_(nullptr),
        num_attribute_data_(0),
        decoder_impl_(nullptr) {}
  void Init(MeshEdgeBreakerDecoderImplInterface *decoder) {
    decoder_impl_ = decoder;
    buffer_.Init(decoder->GetDecoder()->buffer()->data_head(),
                 decoder->GetDecoder()->buffer()->remaining_size());
  }

  // Used to tell the decoder what is the number of expected decoded vertices.
  // Ignored by default.
  void SetNumEncodedVertices(int /* num_vertices */) {}

  // Set the number of non-position attribute data for which we need to decode
  // the connectivity.
  void SetNumAttributeData(int num_data) { num_attribute_data_ = num_data; }

  // Called before the traversal decoding is started.
  // Returns a buffer decoder that points to data that was encoded after the
  // traversal.
  bool Start(DecoderBuffer *out_buffer) {
    // Decode symbols from the main buffer decoder and face configurations from
    // the start_face_buffer decoder.
    uint64_t traversal_size;
    if (!buffer_.StartBitDecoding(true, &traversal_size))
      return false;
    start_face_buffer_.Init(buffer_.data_head(), buffer_.remaining_size());
    if (traversal_size > start_face_buffer_.remaining_size())
      return false;
    start_face_buffer_.Advance(traversal_size);
    if (!start_face_buffer_.StartBitDecoding(true, &traversal_size))
      return false;
    // Create a decoder that is set to the end of the encoded traversal data.
    DecoderBuffer ret;
    ret.Init(start_face_buffer_.data_head(),
             start_face_buffer_.remaining_size());
    if (traversal_size > ret.remaining_size())
      return false;
    ret.Advance(traversal_size);
    // Prepare attribute decoding.
    if (num_attribute_data_ > 0) {
      attribute_connectivity_decoders_ = std::unique_ptr<BinaryDecoder[]>(
          new BinaryDecoder[num_attribute_data_]);
      for (int i = 0; i < num_attribute_data_; ++i) {
        if (!attribute_connectivity_decoders_[i].StartDecoding(&ret))
          return false;
      }
    }
    *out_buffer = ret;
    return true;
  }

  // Returns the configuration of a new initial face.
  inline bool DecodeStartFaceConfiguration() {
    uint32_t face_configuration;
    start_face_buffer_.DecodeLeastSignificantBits32(1, &face_configuration);
    return face_configuration;
  }

  // Returns the next edgebreaker symbol that was reached during the traversal.
  inline uint32_t DecodeSymbol() {
    uint32_t symbol;
    buffer_.DecodeLeastSignificantBits32(1, &symbol);
    if (symbol == TOPOLOGY_C) {
      return symbol;
    }
    // Else decode two additional bits.
    uint32_t symbol_suffix;
    buffer_.DecodeLeastSignificantBits32(2, &symbol_suffix);
    symbol |= (symbol_suffix << 1);
    return symbol;
  }

  // Called whenever a new active corner is set in the decoder.
  inline void NewActiveCornerReached(CornerIndex /* corner */) {}

  // Called whenever |source| vertex is about to be merged into the |dest|
  // vertex.
  inline void MergeVertices(VertexIndex /* dest */, VertexIndex /* source */) {}

  // Returns true if there is an attribute seam for the next processed pair
  // of visited faces.
  // |attribute| is used to mark the id of the non-position attribute (in range
  // of <0, num_attributes - 1>).
  inline bool DecodeAttributeSeam(int attribute) {
    return attribute_connectivity_decoders_[attribute].DecodeNextBit();
  }

  // Called when the traversal is finished.
  void Done() {
    buffer_.EndBitDecoding();
    start_face_buffer_.EndBitDecoding();
  }

 private:
  // Buffer that contains the encoded data.
  DecoderBuffer buffer_;
  DecoderBuffer start_face_buffer_;
  std::unique_ptr<BinaryDecoder[]> attribute_connectivity_decoders_;
  int num_attribute_data_;
  const MeshEdgeBreakerDecoderImplInterface *decoder_impl_;
};

}  // namespace draco

#endif  // DRACO_COMPRESSION_MESH_MESH_EDGEBREAKER_TRAVERSAL_DECODER_H_
