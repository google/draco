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
#ifndef DRACO_COMPRESSION_MESH_MESH_EDGEBREAKER_SHARED_H_
#define DRACO_COMPRESSION_MESH_MESH_EDGEBREAKER_SHARED_H_

#include <inttypes.h>

namespace draco {

// Shared declarations used by both edgebreaker encoder and decoder.

// A variable length encoding for storing all possible topology configurations
// during traversal of mesh's surface. The configurations are based on visited
// state of neighboring triangles around a currently processed face corner.
// Note that about half of the encountered configurations is expected to be of
// type TOPOLOGY_C. It's guaranteed that the encoding will use at most 2 bits
// per triangle for meshes with no holes and up to 6 bits per triangle for
// general meshes. In addition, the encoding will take up to 4 bits per triangle
// for each non-position attribute attached to the mesh.
//
//     *-------*          *-------*          *-------*
//    / \     / \        / \     / \        / \     / \
//   /   \   /   \      /   \   /   \      /   \   /   \
//  /     \ /     \    /     \ /     \    /     \ /     \
// *-------v-------*  *-------v-------*  *-------v-------*
//  \     /x\     /          /x\     /    \     /x\
//   \   /   \   /          /   \   /      \   /   \
//    \ /  C  \ /          /  L  \ /        \ /  R  \
//     *-------*          *-------*          *-------*
//
//     *       *
//    / \     / \
//   /   \   /   \
//  /     \ /     \
// *-------v-------*          v
//  \     /x\     /          /x\
//   \   /   \   /          /   \
//    \ /  S  \ /          /  E  \
//     *-------*          *-------*
//
enum EdgeBreakerTopologyBitPattern {
  TOPOLOGY_C = 0x0,  // 0
  TOPOLOGY_S = 0x1,  // 1 0 0
  TOPOLOGY_L = 0x3,  // 1 1 0
  TOPOLOGY_R = 0x5,  // 1 0 1
  TOPOLOGY_E = 0x7,  // 1 1 1
  // A special symbol that's not actually encoded, but it can be used to mark
  // the initial face that triggers the mesh encoding of a single connected
  // component.
  TOPOLOGY_INIT_FACE,
  // A special value used to indicate an invalid symbol.
  TOPOLOGY_INVALID
};

// Bit-length of symbols in the EdgeBreakerTopologyBitPattern stored as a
// look up table for faster indexing.
constexpr int32_t edge_breaker_topology_bit_pattern_length[] = {1, 3, 0, 3,
                                                                0, 3, 0, 3};

// Types of edges used during mesh traversal relative to the tip vertex of a
// visited triangle.
enum EdgeFaceName : uint8_t { LEFT_FACE_EDGE = 0, RIGHT_FACE_EDGE = 1 };

// Struct used for storing data about a source face that connects to an
// already traversed face that was either the initial face or a face encoded
// with either toplogy S (split) symbol. Such connection can be only caused by
// topology changes on the traversed surface (if its genus != 0, i.e. when the
// surface has topological handles or holes).
// For each occurence of such event we always encode the split symbol id, source
// symbol id and source edge id (left, or right). There will be always exectly
// two occurences of this event for every topological handle on the traversed
// mesh and one occurence for a hole.
struct TopologySplitEventData {
  int32_t split_symbol_id;
  int32_t source_symbol_id;
  // We need to use uint32_t instead of EdgeFaceName because the most recent
  // version of gcc does not allow that when optimizations are turned on.
  uint32_t source_edge : 1;
  uint32_t split_edge : 1;
};

// Hole event is used to store info about the first symbol that reached a
// vertex of so far unvisited hole. This can happen only on either the initial
// face or during a regular traversal when TOPOLOGY_S is encountered.
struct HoleEventData {
  int32_t symbol_id;
  HoleEventData() : symbol_id(0) {}
  explicit HoleEventData(int32_t sym_id) : symbol_id(sym_id) {}
};

}  // namespace draco

#endif  // DRACO_COMPRESSION_MESH_MESH_EDGEBREAKER_SHARED_H_
