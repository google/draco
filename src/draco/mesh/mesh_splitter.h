// Copyright 2017 The Draco Authors.
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
#ifndef DRACO_MESH_MESH_SPLITTER_H_
#define DRACO_MESH_MESH_SPLITTER_H_

#include "draco/draco_features.h"

#ifdef DRACO_TRANSCODER_SUPPORTED
#include "draco/core/status_or.h"
#include "draco/mesh/mesh.h"

namespace draco {

// Class that can be used to split a single mesh into multiple sub-meshes
// according to specified criteria.
class MeshSplitter {
 public:
  typedef std::vector<std::unique_ptr<Mesh>> MeshVector;
  MeshSplitter();

  // Sets a flag that tells the splitter to preserve all materials on the input
  // mesh during mesh splitting. When set, the materials used on sub-meshes are
  // going to be copied over. Any redundant materials on sub-meshes are going to
  // be deleted.
  // Default = false.
  void SetPreserveMaterials(bool flag) { preserve_materials_ = flag; }

  // Splits the input |mesh| according to attribute values stored in the
  // specified attribute. The attribute values need to be defined per-face, that
  // is, all points attached to a single face must share the same attribute
  // value. Each attribute value (AttributeValueIndex) is mapped to a single
  // output mesh. If an AttributeValueIndex is unused, no mesh is created for
  // the given value.
  StatusOr<MeshVector> SplitMesh(const Mesh &mesh, uint32_t split_attribute_id);

 private:
  bool preserve_materials_;
};

}  // namespace draco

#endif  // DRACO_TRANSCODER_SUPPORTED
#endif  // DRACO_MESH_MESH_SPLITTER_H_
