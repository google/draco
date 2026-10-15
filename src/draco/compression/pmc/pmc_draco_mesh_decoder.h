// Copyright 2026 The Draco Authors.
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
#ifndef DRACO_COMPRESSION_PMC_PMC_DRACO_MESH_DECODER_H_
#define DRACO_COMPRESSION_PMC_PMC_DRACO_MESH_DECODER_H_

#include "draco/draco_features.h"

#ifdef DRACO_PMC_SUPPORTED

#include <string>
#include <vector>

#include "draco/core/decoder_buffer.h"
#include "draco/core/status.h"
#include "draco/mesh/mesh.h"

namespace draco {

// Decodes a draco::Mesh from draco::EncoderBuffer in PMC PMC bitstream format.
class PmcDracoMeshDecoder {
 public:
  PmcDracoMeshDecoder() = default;
  ~PmcDracoMeshDecoder() = default;

  // Decodes mesh from buffer. When |attribute_names| are provided, decodes only
  // the attributes with provided names and returns an error if the buffer has
  // missing attribute with provided name.
  Status Decode(DecoderBuffer* in_buffer, Mesh* mesh,
                const std::vector<std::string>& attribute_names = {});
};

}  // namespace draco

#endif  // DRACO_PMC_SUPPORTED
#endif  // DRACO_COMPRESSION_PMC_PMC_DRACO_MESH_DECODER_H_
