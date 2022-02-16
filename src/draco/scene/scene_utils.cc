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

#include "draco/scene/scene_utils.h"

#ifdef DRACO_TRANSCODER_SUPPORTED
#include <numeric>
#include <string>
#include <unordered_set>
#include <utility>

#include "draco/core/hash_utils.h"
#include "draco/core/vector_d.h"
#include "draco/mesh/mesh_splitter.h"
#include "draco/mesh/mesh_utils.h"
#include "draco/scene/scene_indices.h"
#include "draco/texture/texture_utils.h"

namespace draco {

IndexTypeVector<MeshInstanceIndex, SceneUtils::MeshInstance>
SceneUtils::ComputeAllInstances(const Scene &scene) {
  IndexTypeVector<MeshInstanceIndex, MeshInstance> instances;

  // Traverse the scene assuming multiple root nodes.
  const Eigen::Matrix4d transform = Eigen::Matrix4d::Identity();

  struct Node {
    const SceneNodeIndex scene_node_index;
    Eigen::Matrix4d transform;
  };
  std::vector<Node> nodes;
  nodes.reserve(scene.NumRootNodes());
  for (int i = 0; i < scene.NumRootNodes(); ++i) {
    nodes.push_back({scene.GetRootNodeIndex(i), transform});
  }

  while (!nodes.empty()) {
    const Node node = nodes.back();
    nodes.pop_back();
    const SceneNode &scene_node = *scene.GetNode(node.scene_node_index);
    const Eigen::Matrix4d combined_transform =
        node.transform *
        scene_node.GetTrsMatrix().ComputeTransformationMatrix();

    // Create instances from node meshes.
    const MeshGroupIndex mesh_group_index = scene_node.GetMeshGroupIndex();
    if (mesh_group_index != kInvalidMeshGroupIndex) {
      const MeshGroup &mesh_group = *scene.GetMeshGroup(mesh_group_index);
      for (int i = 0; i < mesh_group.NumMeshIndices(); i++) {
        const MeshIndex mesh_index = mesh_group.GetMeshIndex(i);
        if (mesh_index != kInvalidMeshIndex) {
          instances.push_back(
              {mesh_index, node.scene_node_index, i, combined_transform});
        }
      }
    }

    // Traverse children nodes.
    for (int i = 0; i < scene_node.NumChildren(); i++) {
      nodes.push_back({scene_node.Child(i), combined_transform});
    }
  }
  return instances;
}

Eigen::Matrix4d SceneUtils::ComputeGlobalNodeTransform(const Scene &scene,
                                                       SceneNodeIndex index) {
  Eigen::Matrix4d transform = Eigen::Matrix4d::Identity();
  while (index != kInvalidSceneNodeIndex) {
    const SceneNode *const node = scene.GetNode(index);
    transform = node->GetTrsMatrix().ComputeTransformationMatrix() * transform;
    index = node->NumParents() == 1 ? node->Parent(0) : kInvalidSceneNodeIndex;
  }
  return transform;
}

IndexTypeVector<MeshIndex, int> SceneUtils::NumMeshInstances(
    const Scene &scene) {
  const auto instances = ComputeAllInstances(scene);
  IndexTypeVector<MeshIndex, int> num_mesh_instances(scene.NumMeshes(), 0);
  for (MeshInstanceIndex i(0); i < instances.size(); i++) {
    const MeshInstance &instance = instances[i];
    num_mesh_instances[instance.mesh_index]++;
  }
  return num_mesh_instances;
}

int SceneUtils::GetMeshInstanceMaterialIndex(const Scene &scene,
                                             const MeshInstance &instance) {
  const auto *const node = scene.GetNode(instance.scene_node_index);
  return scene.GetMeshGroup(node->GetMeshGroupIndex())
      ->GetMaterialIndex(instance.mesh_group_mesh_index);
}

int SceneUtils::NumFacesOnBaseMeshes(const Scene &scene) {
  int num_faces = 0;
  for (MeshIndex i(0); i < scene.NumMeshes(); ++i) {
    num_faces += scene.GetMesh(i).num_faces();
  }
  return num_faces;
}

int SceneUtils::NumFacesOnInstancedMeshes(const Scene &scene) {
  const auto instances = ComputeAllInstances(scene);
  int num_faces = 0;
  for (MeshInstanceIndex i(0); i < instances.size(); i++) {
    const MeshInstance &instance = instances[i];
    num_faces += scene.GetMesh(instance.mesh_index).num_faces();
  }
  return num_faces;
}

int SceneUtils::NumPointsOnBaseMeshes(const Scene &scene) {
  int num_points = 0;
  for (MeshIndex i(0); i < scene.NumMeshes(); ++i) {
    num_points += scene.GetMesh(i).num_points();
  }
  return num_points;
}

int SceneUtils::NumPointsOnInstancedMeshes(const Scene &scene) {
  const auto instances = ComputeAllInstances(scene);
  int num_points = 0;
  for (MeshInstanceIndex i(0); i < instances.size(); i++) {
    const MeshInstance &instance = instances[i];
    num_points += scene.GetMesh(instance.mesh_index).num_points();
  }
  return num_points;
}

int SceneUtils::NumAttEntriesOnBaseMeshes(const Scene &scene,
                                          GeometryAttribute::Type att_type) {
  int num_att_entries = 0;
  for (MeshIndex i(0); i < scene.NumMeshes(); ++i) {
    const Mesh &mesh = scene.GetMesh(i);
    const PointAttribute *att = mesh.GetNamedAttribute(att_type);
    if (att != nullptr) {
      num_att_entries += att->size();
    }
  }
  return num_att_entries;
}

int SceneUtils::NumAttEntriesOnInstancedMeshes(
    const Scene &scene, GeometryAttribute::Type att_type) {
  const auto instances = ComputeAllInstances(scene);
  int num_att_entries = 0;
  for (MeshInstanceIndex i(0); i < instances.size(); i++) {
    const MeshInstance &instance = instances[i];
    const Mesh &mesh = scene.GetMesh(instance.mesh_index);
    const PointAttribute *att = mesh.GetNamedAttribute(att_type);
    if (att != nullptr) {
      num_att_entries += att->size();
    }
  }
  return num_att_entries;
}

BoundingBox SceneUtils::ComputeBoundingBox(const Scene &scene) {
  // Compute bounding box that includes all scene mesh instances.
  const auto instances = ComputeAllInstances(scene);
  BoundingBox scene_bbox;
  for (MeshInstanceIndex i(0); i < instances.size(); i++) {
    const MeshInstance &instance = instances[i];
    const BoundingBox mesh_bbox =
        ComputeMeshInstanceBoundingBox(scene, instance);
    scene_bbox.Update(mesh_bbox);
  }
  return scene_bbox;
}

BoundingBox SceneUtils::ComputeMeshInstanceBoundingBox(
    const Scene &scene, const MeshInstance &instance) {
  const Mesh &mesh = scene.GetMesh(instance.mesh_index);
  BoundingBox mesh_bbox;
  auto pc_att = mesh.GetNamedAttribute(GeometryAttribute::POSITION);
  Eigen::Vector4d position;
  position[3] = 1.0;
  for (AttributeValueIndex i(0); i < pc_att->size(); ++i) {
    pc_att->ConvertValue<double>(i, &position[0]);
    const Eigen::Vector4d transformed = instance.transform * position;
    mesh_bbox.Update({static_cast<float>(transformed[0]),
                      static_cast<float>(transformed[1]),
                      static_cast<float>(transformed[2])});
  }
  return mesh_bbox;
}

StatusOr<std::unique_ptr<Scene>> SceneUtils::MeshToScene(
    std::unique_ptr<Mesh> mesh) {
  const size_t num_mesh_materials = mesh->GetMaterialLibrary().NumMaterials();
  std::unique_ptr<Scene> scene(new Scene());
  if (num_mesh_materials > 0) {
    scene->GetMaterialLibrary().Copy(mesh->GetMaterialLibrary());
    mesh->GetMaterialLibrary().Clear();
  } else {
    // Create a default material for the scene.
    scene->GetMaterialLibrary().MutableMaterial(0);
  }

  const SceneNodeIndex scene_node_index = scene->AddNode();
  SceneNode *const scene_node = scene->GetNode(scene_node_index);
  const MeshGroupIndex mesh_group_index = scene->AddMeshGroup();
  MeshGroup *const mesh_group = scene->GetMeshGroup(mesh_group_index);

  if (num_mesh_materials <= 1) {
    const MeshIndex mesh_index = scene->AddMesh(std::move(mesh));
    if (mesh_index == kInvalidMeshIndex) {
      // No idea whether this can happen.  It's not covered by any unit test.
      return Status(Status::DRACO_ERROR, "Could not add Draco mesh to scene.");
    }
    mesh_group->AddMeshIndex(mesh_index);
    mesh_group->AddMaterialIndex(0);
  } else {
    const int32_t mat_att_id =
        mesh->GetNamedAttributeId(GeometryAttribute::MATERIAL);
    if (mat_att_id == -1) {
      // Probably dead code, not covered by any unit test.
      return Status(Status::DRACO_ERROR,
                    "Internal error in MeshToScene: "
                    "GetNamedAttributeId(MATERIAL) returned -1");
    }
    const PointAttribute *const mat_att =
        mesh->GetNamedAttribute(GeometryAttribute::MATERIAL);
    if (mat_att == nullptr) {
      // Probably dead code, not covered by any unit test.
      return Status(Status::DRACO_ERROR,
                    "Internal error in MeshToScene: "
                    "GetNamedAttribute(MATERIAL) returned nullptr");
    }

    MeshSplitter splitter;
    DRACO_ASSIGN_OR_RETURN(MeshSplitter::MeshVector split_meshes,
                           splitter.SplitMesh(*mesh, mat_att_id));
    // Note: cannot clear mesh here, since mat_att points into it.
    for (size_t i = 0; i < split_meshes.size(); ++i) {
      if (split_meshes[i] == nullptr) {
        // Probably dead code, not covered by any unit test.
        continue;
      }
      const MeshIndex mesh_index = scene->AddMesh(std::move(split_meshes[i]));
      if (mesh_index == kInvalidMeshIndex) {
        // No idea whether this can happen.  It's not covered by any unit test.
        return Status(Status::DRACO_ERROR,
                      "Could not add Draco mesh to scene.");
      }

      uint32_t material_index = 0;
      mat_att->GetValue(AttributeValueIndex(i), &material_index);

      mesh_group->AddMeshIndex(mesh_index);
      mesh_group->AddMaterialIndex(material_index);
    }
  }

  scene_node->SetMeshGroupIndex(mesh_group_index);
  scene->AddRootNodeIndex(scene_node_index);
  return scene;
}

void SceneUtils::PrintInfo(const Scene &input, const Scene &simplified,
                           bool verbose) {
  struct Printer {
    Printer(const Scene &input, const Scene &simplified)
        : input(input), simplified(simplified), print_instanced_info(false) {
      // Info about the instanced meshes is printed if some of the meshes have
      // multiple instances and also if the number of base meshes has changed.
      auto input_instances = SceneUtils::NumMeshInstances(input);
      auto simplified_instances = SceneUtils::NumMeshInstances(simplified);
      if (input_instances.size() != simplified_instances.size()) {
        print_instanced_info = true;
        return;
      }
      for (MeshIndex i(0); i < input_instances.size(); i++) {
        if (input_instances[i] != 1 || simplified_instances[i] != 1) {
          print_instanced_info = true;
          return;
        }
      }
    }

    void PrintInfoHeader() const {
      printf("\n");
      printf("%21s |   geometry:         base", "");
      if (print_instanced_info) {
        printf("    instanced");
      }
      printf("\n");
    }

    void PrintInfoRow(const std::string &label, int count_input_base,
                      int count_input_instanced, int count_simplified_base,
                      int count_simplified_instanced) const {
      // Do not clutter the printout with empty info.
      if (count_input_base == 0 && count_input_instanced == 0) {
        return;
      }
      printf("  ----------------------------------------------");
      if (print_instanced_info) {
        printf("-------------");
      }
      printf("\n");
      printf("%21s |      input: %12d", label.c_str(), count_input_base);
      if (print_instanced_info) {
        printf(" %12d", count_input_instanced);
      }
      printf("\n");
      printf("%21s | simplified: %12d", "", count_simplified_base);
      if (print_instanced_info) {
        printf(" %12d", count_simplified_instanced);
      }
      printf("\n");
    }

    void PrintAttInfoRow(const std::string &label, const draco::Scene &input,
                         const draco::Scene &simplified,
                         draco::GeometryAttribute::Type att_type) const {
      PrintInfoRow(label, NumAttEntriesOnBaseMeshes(input, att_type),
                   NumAttEntriesOnInstancedMeshes(input, att_type),
                   NumAttEntriesOnBaseMeshes(simplified, att_type),
                   NumAttEntriesOnInstancedMeshes(simplified, att_type));
    }

    const Scene &input;
    const Scene &simplified;
    bool print_instanced_info;
  };

  // Print information about input and simplified scenes.
  const Printer printer(input, simplified);
  printer.PrintInfoHeader();
  if (verbose) {
    const int num_meshes_input_base = input.NumMeshes();
    const int num_meshes_simplified_base = simplified.NumMeshes();
    const int num_meshes_input_instanced = ComputeAllInstances(input).size();
    const int num_meshes_simplified_instanced =
        ComputeAllInstances(simplified).size();
    printer.PrintInfoRow("Number of meshes", num_meshes_input_base,
                         num_meshes_input_instanced, num_meshes_simplified_base,
                         num_meshes_simplified_instanced);
  }
  printer.PrintInfoRow("Number of faces", NumFacesOnBaseMeshes(input),
                       NumFacesOnInstancedMeshes(input),
                       NumFacesOnBaseMeshes(simplified),
                       NumFacesOnInstancedMeshes(simplified));
  if (verbose) {
    printer.PrintInfoRow("Number of points", NumPointsOnBaseMeshes(input),
                         NumPointsOnInstancedMeshes(input),
                         NumPointsOnBaseMeshes(simplified),
                         NumPointsOnInstancedMeshes(simplified));
    printer.PrintAttInfoRow("Number of positions", input, simplified,
                            draco::GeometryAttribute::POSITION);
    printer.PrintAttInfoRow("Number of normals", input, simplified,
                            draco::GeometryAttribute::NORMAL);
    printer.PrintAttInfoRow("Number of colors", input, simplified,
                            draco::GeometryAttribute::COLOR);
    printer.PrintInfoRow("Number of materials",
                         input.GetMaterialLibrary().NumMaterials(),
                         simplified.GetMaterialLibrary().NumMaterials(),
                         input.GetMaterialLibrary().NumMaterials(),
                         simplified.GetMaterialLibrary().NumMaterials());
  }
}

StatusOr<std::unique_ptr<Mesh>> SceneUtils::InstantiateMesh(
    const Scene &scene, const MeshInstance &instance) {
  // Check if the |scene| has base mesh corresponding to mesh |instance|.
  if (scene.NumMeshes() <= instance.mesh_index.value()) {
    Status(Status::DRACO_ERROR, "Scene has no corresponding base mesh.");
  }

  // Check that mesh has valid positions.
  const Mesh &base_mesh = scene.GetMesh(instance.mesh_index);
  const int32_t pos_id =
      base_mesh.GetNamedAttributeId(GeometryAttribute::POSITION);
  const PointAttribute *const pos_att = base_mesh.attribute(pos_id);
  if (pos_att == nullptr) {
    return Status(Status::DRACO_ERROR, "Mesh has no positions.");
  }
  if (pos_att->data_type() != DT_FLOAT32 || pos_att->num_components() != 3) {
    return Status(Status::DRACO_ERROR, "Mesh has invalid positions.");
  }

  // Copy the base mesh from |scene|.
  std::unique_ptr<Mesh> mesh(new Mesh());
  mesh->Copy(base_mesh);

  // Apply transformation to mesh unless transformation is identity.
  if (instance.transform != Eigen::Matrix4d::Identity()) {
    MeshUtils::TransformMesh(instance.transform, mesh.get());
  }
  return mesh;
}

void SceneUtils::Cleanup(Scene *scene) {
  // Remove invalid mesh indices from mesh groups.
  for (MeshGroupIndex i(0); i < scene->NumMeshGroups(); i++) {
    scene->GetMeshGroup(i)->RemoveMeshIndex(kInvalidMeshIndex);
  }

  // Find references to mesh groups.
  std::vector<bool> is_mesh_group_referenced(scene->NumMeshGroups(), false);
  for (SceneNodeIndex i(0); i < scene->NumNodes(); i++) {
    const SceneNode &node = *scene->GetNode(i);
    const MeshGroupIndex mesh_group_index = node.GetMeshGroupIndex();
    if (mesh_group_index != kInvalidMeshGroupIndex) {
      is_mesh_group_referenced[mesh_group_index.value()] = true;
    }
  }

  // Find references to base meshes from referenced mesh groups and find mesh
  // groups that have no valid references to base meshes.
  std::vector<bool> is_base_mesh_referenced(scene->NumMeshes(), false);
  std::vector<bool> is_mesh_group_empty(scene->NumMeshGroups(), false);
  for (MeshGroupIndex i(0); i < scene->NumMeshGroups(); i++) {
    if (!is_mesh_group_referenced[i.value()]) {
      continue;
    }
    const MeshGroup &mesh_group = *scene->GetMeshGroup(i);
    bool mesh_group_is_empty = true;
    for (int j = 0; j < mesh_group.NumMeshIndices(); j++) {
      const MeshIndex base_mesh_index = mesh_group.GetMeshIndex(j);
      mesh_group_is_empty = false;
      is_base_mesh_referenced[base_mesh_index.value()] = true;
    }
    if (mesh_group_is_empty) {
      is_mesh_group_empty[i.value()] = true;
    }
  }

  // Remove base meshes with no references to them.
  for (int i = scene->NumMeshes() - 1; i >= 0; i--) {
    const MeshIndex mi(i);
    if (!is_base_mesh_referenced[mi.value()]) {
      scene->RemoveMesh(mi);
    }
  }

  // Remove empty mesh groups with no geometry or no references to them.
  for (int i = scene->NumMeshGroups() - 1; i >= 0; i--) {
    const MeshGroupIndex mgi(i);
    if (is_mesh_group_empty[mgi.value()] ||
        !is_mesh_group_referenced[mgi.value()]) {
      scene->RemoveMeshGroup(mgi);
    }
  }

  // Find materials that reference a texture.
  const MaterialLibrary &material_library = scene->GetMaterialLibrary();
  std::vector<bool> materials_with_textures(material_library.NumMaterials(),
                                            false);
  for (int i = 0; i < material_library.NumMaterials(); ++i) {
    if (material_library.GetMaterial(i)->NumTextureMaps() > 0) {
      materials_with_textures[i] = true;
    }
  }

  // Find which materials have a refernece to them.
  std::vector<bool> is_material_referenced(material_library.NumMaterials(),
                                           false);
  // Remove TEX_COORD attributes for meshes that reference a material that does
  // not contain any texture maps.
  // TODO(fgalligan): Remove TEX_COORD attributes for meshes that references a
  // material that does not reference the specific texture. E.g. A mesh has two
  // TEX_COORD attributes but the material only uses one of the TEX_COORD
  // attributes.
  for (int mgi = 0; mgi < scene->NumMeshGroups(); ++mgi) {
    const MeshGroup *const mesh_group =
        scene->GetMeshGroup(MeshGroupIndex(mgi));
    for (int mi = 0; mi < mesh_group->NumMeshIndices(); ++mi) {
      const MeshIndex mesh_index = mesh_group->GetMeshIndex(mi);

      const int material_index = mesh_group->GetMaterialIndex(mi);
      if (material_index > -1) {
        is_material_referenced[material_index] = true;

        if (!materials_with_textures[material_index]) {
          Mesh &mesh = scene->GetMesh(mesh_index);
          // |mesh| references a material that does not have a texture.
          while (mesh.NumNamedAttributes(GeometryAttribute::TEX_COORD) > 0) {
            mesh.DeleteAttribute(
                mesh.GetNamedAttributeId(GeometryAttribute::TEX_COORD, 0));
          }
        }
      }
    }
  }

  // Remove materials with no reference to them.
  for (int i = material_library.NumMaterials() - 1; i >= 0; --i) {
    if (!is_material_referenced[i]) {
      // Material |i| is not referenced.
      scene->RemoveMaterial(i);
    }
  }
}

void SceneUtils::RemoveMeshInstances(const std::vector<MeshInstance> &instances,
                                     Scene *scene) {
  // Remove mesh instances from the scene.
  for (const SceneUtils::MeshInstance &instance : instances) {
    const MeshGroupIndex mgi =
        scene->GetNode(instance.scene_node_index)->GetMeshGroupIndex();

    // Create a new mesh group with removed instance (we can't just delete the
    // instance from the mesh group directly, because the same mesh group may
    // be used by multiple scene nodes).
    const MeshGroupIndex new_mesh_group_index = scene->AddMeshGroup();
    MeshGroup &new_mesh_group = *scene->GetMeshGroup(new_mesh_group_index);

    new_mesh_group.Copy(*scene->GetMeshGroup(mgi));
    new_mesh_group.RemoveMeshIndex(instance.mesh_index);

    // Assign the new mesh group to the scene node. Unused mesh groups will be
    // automatically removed later during a scene cleanup operation.
    scene->GetNode(instance.scene_node_index)
        ->SetMeshGroupIndex(new_mesh_group_index);
  }

  // Remove duplicate mesh groups that may have been created during the instance
  // removal process.
  DeduplicateMeshGroups(scene);
}

void SceneUtils::DeduplicateMeshGroups(Scene *scene) {
  if (scene->NumMeshGroups() <= 1) {
    return;
  }

  // Signature of a mesh group used for detecting duplicates.
  struct MeshGroupSignature {
    const MeshGroupIndex mesh_group_index;
    const MeshGroup &mesh_group;
    MeshGroupSignature(MeshGroupIndex mgi, const MeshGroup &mesh_group)
        : mesh_group_index(mgi), mesh_group(mesh_group) {}

    bool operator==(const MeshGroupSignature &signature) const {
      if (mesh_group.GetName() != signature.mesh_group.GetName()) {
        return false;
      }
      if (mesh_group.NumMeshIndices() !=
          signature.mesh_group.NumMeshIndices()) {
        return false;
      }
      if (mesh_group.NumMaterialIndices() !=
          signature.mesh_group.NumMaterialIndices()) {
        return false;
      }
      // TODO(ostava): We may consider sorting meshes within a mesh group to
      // make the order of meshes irrelevant. This should be done only for
      // meshes with opaque materials though, because for transparent
      // geometries, the order matters.
      for (int i = 0; i < mesh_group.NumMeshIndices(); ++i) {
        if (mesh_group.GetMeshIndex(i) !=
            signature.mesh_group.GetMeshIndex(i)) {
          return false;
        }
        if (mesh_group.NumMaterialIndices() > 0) {
          if (mesh_group.GetMaterialIndex(i) !=
              signature.mesh_group.GetMaterialIndex(i)) {
            return false;
          }
        }
      }
      return true;
    }
    struct Hash {
      size_t operator()(const MeshGroupSignature &signature) const {
        size_t hash = 79;  // Magic number.
        hash = HashCombine(signature.mesh_group.GetName(), hash);
        hash = HashCombine(signature.mesh_group.NumMeshIndices(), hash);
        for (int i = 0; i < signature.mesh_group.NumMeshIndices(); ++i) {
          hash = HashCombine(signature.mesh_group.GetMeshIndex(i), hash);
        }
        for (int i = 0; i < signature.mesh_group.NumMaterialIndices(); ++i) {
          hash = HashCombine(signature.mesh_group.GetMaterialIndex(i), hash);
        }
        return hash;
      }
    };
  };

  // Set holding unique mesh groups.
  std::unordered_set<MeshGroupSignature, MeshGroupSignature::Hash>
      unique_mesh_groups;
  IndexTypeVector<MeshGroupIndex, MeshGroupIndex> parent_mesh_group(
      scene->NumMeshGroups());
  for (MeshGroupIndex mgi(0); mgi < scene->NumMeshGroups(); ++mgi) {
    const MeshGroup *mg = scene->GetMeshGroup(mgi);
    const MeshGroupSignature signature(mgi, *mg);
    auto it = unique_mesh_groups.find(signature);
    if (it != unique_mesh_groups.end()) {
      parent_mesh_group[mgi] = it->mesh_group_index;
    } else {
      parent_mesh_group[mgi] = kInvalidMeshGroupIndex;
      unique_mesh_groups.insert(signature);
    }
  }

  // Go over all nodes and update mesh groups if needed.
  for (SceneNodeIndex sni(0); sni < scene->NumNodes(); ++sni) {
    const MeshGroupIndex mgi = scene->GetNode(sni)->GetMeshGroupIndex();
    if (mgi == kInvalidMeshGroupIndex ||
        parent_mesh_group[mgi] == kInvalidMeshGroupIndex) {
      continue;  // Nothing to update.
    }
    scene->GetNode(sni)->SetMeshGroupIndex(parent_mesh_group[mgi]);
  }

  // Remove any unused mesh groups.
  Cleanup(scene);
}

void SceneUtils::SetDracoCompressionOptions(
    const DracoCompressionOptions *options, Scene *scene) {
  for (MeshIndex i(0); i < scene->NumMeshes(); ++i) {
    Mesh &mesh = scene->GetMesh(i);
    if (options == nullptr) {
      mesh.SetCompressionEnabled(false);
    } else {
      mesh.SetCompressionEnabled(true);
      mesh.SetCompressionOptions(*options);
    }
  }
}

bool SceneUtils::IsDracoCompressionEnabled(const Scene &scene) {
  for (MeshIndex i(0); i < scene.NumMeshes(); ++i) {
    if (scene.GetMesh(i).IsCompressionEnabled()) {
      return true;
    }
  }
  return false;
}

}  // namespace draco

#endif  // DRACO_TRANSCODER_SUPPORTED
