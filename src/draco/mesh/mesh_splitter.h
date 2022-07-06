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
#include "draco/mesh/mesh_connected_components.h"
#include "draco/mesh/triangle_soup_mesh_builder.h"

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

  // Splits the input |mesh| into separate components defined in
  // |connected_components|. That is, all faces associated with a given
  // component index will be stored in the same mesh. The number of generated
  // meshes will correspond to |connected_components.NumConnectedComponents()|.
  StatusOr<MeshVector> SplitMeshToComponents(
      const Mesh &mesh, const MeshConnectedComponents &connected_components);

 private:
  struct WorkData {
    // Map between attribute ids of the input and output meshes.
    std::vector<int> att_id_map;
    std::vector<int> num_sub_mesh_faces;
    std::vector<TriangleSoupMeshBuilder> mesh_builders;
  };

  void InitializeMeshBuilder(int mb_index, int num_faces, const Mesh &mesh,
                             int ignored_attribute_id,
                             WorkData *work_data) const;
  void AddFaceToMeshBuilder(int mb_index, FaceIndex source_fi,
                            FaceIndex target_fi, const Mesh &mesh,
                            WorkData *work_data) const;
  StatusOr<MeshVector> FinalizeMeshes(const Mesh &mesh,
                                      WorkData *work_data) const;

  bool preserve_materials_;
};

}  // namespace draco

#endif  // DRACO_TRANSCODER_SUPPORTED
#endif  // DRACO_MESH_MESH_SPLITTER_H_
