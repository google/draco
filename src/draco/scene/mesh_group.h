// Copyright 2019 The Draco Authors.
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
#ifndef DRACO_SCENE_MESH_GROUP_H_
#define DRACO_SCENE_MESH_GROUP_H_

#include "draco/draco_features.h"

#ifdef DRACO_TRANSCODER_SUPPORTED
#include <vector>

#include "draco/core/macros.h"
#include "draco/scene/scene_indices.h"

namespace draco {

// This class is used to hold ordered indices to one or more meshes and
// materials.
class MeshGroup {
 public:
  MeshGroup() {}

  void Copy(const MeshGroup &mg) {
    name_ = mg.name_;
    mesh_indices_ = mg.mesh_indices_;
    material_indices_ = mg.material_indices_;
  }

  const std::string &GetName() const { return name_; }
  void SetName(const std::string &name) { name_ = name; }

  // Add an index to the end.
  void AddMeshIndex(MeshIndex index) { mesh_indices_.push_back(index); }
  void SetMeshIndex(int index, MeshIndex mi) { mesh_indices_[index] = mi; }

  // Returns the mesh index at |index|.
  MeshIndex GetMeshIndex(int index) const { return mesh_indices_[index]; }

  // Returns the number of indices.
  int NumMeshIndices() const { return mesh_indices_.size(); }

  // Removes all occurrences of |mesh_index| from |mesh_indices_| as well as
  // corresponding material indices from |material_indices_|.
  void RemoveMeshIndex(MeshIndex mesh_index) {
    DRACO_DCHECK(material_indices_.empty() ||
                 material_indices_.size() == mesh_indices_.size());
    int i = 0;
    while (i != mesh_indices_.size()) {
      if (mesh_indices_[i] == mesh_index) {
        mesh_indices_.erase(mesh_indices_.begin() + i);
        if (!material_indices_.empty()) {
          material_indices_.erase(material_indices_.begin() + i);
        }
      } else {
        i++;
      }
    }
  }

  // Add an index to the end.
  void AddMaterialIndex(int index) { material_indices_.push_back(index); }

  // Returns the material index at |index|.
  int GetMaterialIndex(int index) const { return material_indices_[index]; }
  void SetMaterialIndex(int index, int mi) { material_indices_[index] = mi; }

  // Returns the number of indices.
  int NumMaterialIndices() const { return material_indices_.size(); }

 private:
  std::string name_;
  std::vector<MeshIndex> mesh_indices_;
  std::vector<int> material_indices_;
};

}  // namespace draco

#endif  // DRACO_TRANSCODER_SUPPORTED
#endif  // DRACO_SCENE_MESH_GROUP_H_
