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
#include "draco/scene/scene.h"

#include <utility>

#ifdef DRACO_TRANSCODER_SUPPORTED
#include "draco/core/macros.h"
#include "draco/scene/scene_indices.h"

namespace draco {

void Scene::Copy(const Scene &s) {
  meshes_.resize(s.meshes_.size());
  for (MeshIndex i(0); i < meshes_.size(); ++i) {
    meshes_[i] = std::unique_ptr<Mesh>(new Mesh());
    meshes_[i]->Copy(*s.meshes_[i]);
  }

  mesh_groups_.resize(s.mesh_groups_.size());
  for (MeshGroupIndex i(0); i < mesh_groups_.size(); ++i) {
    mesh_groups_[i] = std::unique_ptr<MeshGroup>(new MeshGroup());
    mesh_groups_[i]->Copy(*s.mesh_groups_[i]);
  }

  nodes_.resize(s.nodes_.size());
  for (SceneNodeIndex i(0); i < nodes_.size(); ++i) {
    nodes_[i] = std::unique_ptr<SceneNode>(new SceneNode());
    nodes_[i]->Copy(*s.nodes_[i]);
  }

  root_node_indices_ = s.root_node_indices_;

  animations_.resize(s.animations_.size());
  for (AnimationIndex i(0); i < animations_.size(); ++i) {
    animations_[i] = std::unique_ptr<Animation>(new Animation());
    animations_[i]->Copy(*s.animations_[i]);
  }

  skins_.resize(s.skins_.size());
  for (SkinIndex i(0); i < skins_.size(); ++i) {
    skins_[i] = std::unique_ptr<Skin>(new Skin());
    skins_[i]->Copy(*s.skins_[i]);
  }

  lights_.resize(s.lights_.size());
  for (LightIndex i(0); i < lights_.size(); ++i) {
    lights_[i] = std::unique_ptr<Light>(new Light());
    lights_[i]->Copy(*s.lights_[i]);
  }

  material_library_.Copy(s.material_library_);
}

Status Scene::RemoveMesh(MeshIndex index) {
  // Remove base mesh at |index| from |meshes_| and corresponding material index
  // from |mesh_material_indices_|.
  const int new_num_meshes = meshes_.size() - 1;
  for (MeshIndex i(index); i < new_num_meshes; i++) {
    meshes_[i] = std::move(meshes_[i + 1]);
  }
  meshes_.resize(new_num_meshes);

  // Remove references to removed base mesh and corresponding materials from
  // mesh groups, and update references to remaining base meshes in mesh groups.
  for (MeshGroupIndex mgi(0); mgi < NumMeshGroups(); ++mgi) {
    MeshGroup *mesh_group = GetMeshGroup(mgi);
    if (!mesh_group) {
      return Status(Status::DRACO_ERROR, "MeshGroup is null.");
    }
    mesh_group->RemoveMeshIndex(index);
    for (int i = 0; i < mesh_group->NumMeshIndices(); ++i) {
      const MeshIndex mi = mesh_group->GetMeshIndex(i);
      if (mi > index && mi != kInvalidMeshIndex) {
        mesh_group->SetMeshIndex(i, mi - 1);
      }
    }
  }
  return OkStatus();
}

Status Scene::RemoveMeshGroup(MeshGroupIndex index) {
  // Remove mesh group at |index| from |mesh_groups_| vector.
  const int new_num_mesh_groups = mesh_groups_.size() - 1;
  for (MeshGroupIndex i(index); i < new_num_mesh_groups; i++) {
    mesh_groups_[i] = std::move(mesh_groups_[i + 1]);
  }
  mesh_groups_.resize(new_num_mesh_groups);

  // Invalidate references to removed mesh group in scene nodes, and update
  // references to remaining mesh groups in scene nodes.
  for (SceneNodeIndex sni(0); sni < NumNodes(); ++sni) {
    SceneNode *node = GetNode(sni);
    if (!node) {
      return Status(Status::DRACO_ERROR, "Node is null.");
    }
    const MeshGroupIndex mgi = node->GetMeshGroupIndex();
    if (mgi == index) {
      // TODO(vytyaz): Remove the node if possible, e.g., when node has no
      // geometry, no child nodes, no skins, and no lights.
      node->SetMeshGroupIndex(kInvalidMeshGroupIndex);
    } else if (mgi > index && mgi != kInvalidMeshGroupIndex) {
      node->SetMeshGroupIndex(mgi - 1);
    }
  }
  return OkStatus();
}

Status Scene::RemoveMaterial(int index) {
  if (index < 0 || index >= material_library_.NumMaterials()) {
    return Status(Status::DRACO_ERROR, "Material index is out of range.");
  }
  material_library_.RemoveMaterial(index);

  // Update material indices of mesh instances.
  for (MeshGroupIndex mgi(0); mgi < NumMeshGroups(); ++mgi) {
    MeshGroup *const mesh_group = GetMeshGroup(mgi);
    for (int i = 0; i < mesh_group->NumMaterialIndices(); i++) {
      const int material_index = mesh_group->GetMaterialIndex(i);
      if (material_index > index) {
        mesh_group->SetMaterialIndex(i, material_index - 1);
      } else if (material_index == index) {
        return Status(Status::DRACO_ERROR, "Removed material has references.");
      }
    }
  }
  return OkStatus();
}

}  // namespace draco

#endif  // DRACO_TRANSCODER_SUPPORTED
