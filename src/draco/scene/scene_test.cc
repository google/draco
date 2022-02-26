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

#include "draco/core/draco_test_base.h"
#include "draco/core/draco_test_utils.h"
#include "draco/mesh/mesh_are_equivalent.h"
#include "draco/scene/scene_indices.h"

namespace {

#ifdef DRACO_TRANSCODER_SUPPORTED

TEST(SceneTest, TestCopy) {
  // Test copying of scene data.
  auto src_scene =
      draco::ReadSceneFromTestFile("CesiumMilkTruck/glTF/CesiumMilkTruck.gltf");
  ASSERT_NE(src_scene, nullptr);

  draco::Scene dst_scene;
  dst_scene.Copy(*src_scene);

  ASSERT_EQ(src_scene->NumMeshes(), dst_scene.NumMeshes());
  ASSERT_EQ(src_scene->NumMeshGroups(), dst_scene.NumMeshGroups());
  ASSERT_EQ(src_scene->NumNodes(), dst_scene.NumNodes());
  ASSERT_EQ(src_scene->NumAnimations(), dst_scene.NumAnimations());
  ASSERT_EQ(src_scene->NumSkins(), dst_scene.NumSkins());
  ASSERT_EQ(src_scene->NumLights(), dst_scene.NumLights());

  for (draco::MeshIndex i(0); i < src_scene->NumMeshes(); ++i) {
    draco::MeshAreEquivalent eq;
    ASSERT_TRUE(eq(src_scene->GetMesh(i), dst_scene.GetMesh(i)));
  }
  for (draco::MeshGroupIndex i(0); i < src_scene->NumMeshGroups(); ++i) {
    ASSERT_EQ(src_scene->GetMeshGroup(i)->NumMeshIndices(),
              dst_scene.GetMeshGroup(i)->NumMeshIndices());
    for (int j = 0; j < src_scene->GetMeshGroup(i)->NumMeshIndices(); ++j) {
      ASSERT_EQ(src_scene->GetMeshGroup(i)->GetMeshIndex(j),
                dst_scene.GetMeshGroup(i)->GetMeshIndex(j));
      ASSERT_EQ(src_scene->GetMeshGroup(i)->GetMaterialIndex(j),
                dst_scene.GetMeshGroup(i)->GetMaterialIndex(j));
    }
  }
  for (draco::SceneNodeIndex i(0); i < src_scene->NumNodes(); ++i) {
    ASSERT_EQ(src_scene->GetNode(i)->NumParents(),
              dst_scene.GetNode(i)->NumParents());
    for (int j = 0; j < src_scene->GetNode(i)->NumParents(); ++j) {
      ASSERT_EQ(src_scene->GetNode(i)->Parent(j),
                dst_scene.GetNode(i)->Parent(j));
    }
    ASSERT_EQ(src_scene->GetNode(i)->NumChildren(),
              dst_scene.GetNode(i)->NumChildren());
    for (int j = 0; j < src_scene->GetNode(i)->NumChildren(); ++j) {
      ASSERT_EQ(src_scene->GetNode(i)->Child(j),
                dst_scene.GetNode(i)->Child(j));
    }
    ASSERT_EQ(src_scene->GetNode(i)->GetMeshGroupIndex(),
              dst_scene.GetNode(i)->GetMeshGroupIndex());
    ASSERT_EQ(src_scene->GetNode(i)->GetSkinIndex(),
              dst_scene.GetNode(i)->GetSkinIndex());
    ASSERT_EQ(src_scene->GetNode(i)->GetLightIndex(),
              dst_scene.GetNode(i)->GetLightIndex());
  }
}

TEST(SceneTest, TestRemoveMesh) {
  // Test that a base mesh can be removed from scene.
  auto src_scene_ptr =
      draco::ReadSceneFromTestFile("CesiumMilkTruck/glTF/CesiumMilkTruck.gltf");
  ASSERT_NE(src_scene_ptr, nullptr);
  const draco::Scene &src_scene = *src_scene_ptr;

  // Copy scene.
  draco::Scene dst_scene;
  dst_scene.Copy(src_scene);
  ASSERT_EQ(dst_scene.NumMeshes(), 4);
  draco::MeshAreEquivalent eq;
  ASSERT_TRUE(eq(dst_scene.GetMesh(draco::MeshIndex(0)),
                 src_scene.GetMesh(draco::MeshIndex(0))));
  ASSERT_TRUE(eq(dst_scene.GetMesh(draco::MeshIndex(1)),
                 src_scene.GetMesh(draco::MeshIndex(1))));
  ASSERT_TRUE(eq(dst_scene.GetMesh(draco::MeshIndex(2)),
                 src_scene.GetMesh(draco::MeshIndex(2))));
  ASSERT_TRUE(eq(dst_scene.GetMesh(draco::MeshIndex(3)),
                 src_scene.GetMesh(draco::MeshIndex(3))));

  // Remove base mesh from scene.
  dst_scene.RemoveMesh(draco::MeshIndex(2));
  ASSERT_EQ(dst_scene.NumMeshes(), 3);
  ASSERT_TRUE(eq(dst_scene.GetMesh(draco::MeshIndex(0)),
                 src_scene.GetMesh(draco::MeshIndex(0))));
  ASSERT_TRUE(eq(dst_scene.GetMesh(draco::MeshIndex(1)),
                 src_scene.GetMesh(draco::MeshIndex(1))));
  ASSERT_TRUE(eq(dst_scene.GetMesh(draco::MeshIndex(2)),
                 src_scene.GetMesh(draco::MeshIndex(3))));

  // Remove another base mesh from scene.
  dst_scene.RemoveMesh(draco::MeshIndex(1));
  ASSERT_EQ(dst_scene.NumMeshes(), 2);
  ASSERT_TRUE(eq(dst_scene.GetMesh(draco::MeshIndex(0)),
                 src_scene.GetMesh(draco::MeshIndex(0))));
  ASSERT_TRUE(eq(dst_scene.GetMesh(draco::MeshIndex(1)),
                 src_scene.GetMesh(draco::MeshIndex(3))));
}

TEST(SceneTest, TestRemoveMeshGroup) {
  // Test that a mesh group can be removed from scene.
  auto src_scene_ptr =
      draco::ReadSceneFromTestFile("CesiumMilkTruck/glTF/CesiumMilkTruck.gltf");
  ASSERT_NE(src_scene_ptr, nullptr);
  const draco::Scene &src_scene = *src_scene_ptr;

  // Copy scene.
  draco::Scene dst_scene;
  dst_scene.Copy(src_scene);
  ASSERT_EQ(dst_scene.NumMeshGroups(), 2);
  ASSERT_EQ(dst_scene.NumNodes(), 5);
  ASSERT_EQ(dst_scene.GetNode(draco::SceneNodeIndex(0))->GetMeshGroupIndex(),
            draco::MeshGroupIndex(0));
  ASSERT_EQ(dst_scene.GetNode(draco::SceneNodeIndex(2))->GetMeshGroupIndex(),
            draco::MeshGroupIndex(1));
  ASSERT_EQ(dst_scene.GetNode(draco::SceneNodeIndex(4))->GetMeshGroupIndex(),
            draco::MeshGroupIndex(1));

  // Remove mesh group from scene.
  dst_scene.RemoveMeshGroup(draco::MeshGroupIndex(0));
  ASSERT_EQ(dst_scene.NumMeshGroups(), 1);
  ASSERT_EQ(dst_scene.NumNodes(), 5);
  ASSERT_EQ(dst_scene.GetNode(draco::SceneNodeIndex(0))->GetMeshGroupIndex(),
            draco::kInvalidMeshGroupIndex);
  ASSERT_EQ(dst_scene.GetNode(draco::SceneNodeIndex(2))->GetMeshGroupIndex(),
            draco::MeshGroupIndex(0));
  ASSERT_EQ(dst_scene.GetNode(draco::SceneNodeIndex(4))->GetMeshGroupIndex(),
            draco::MeshGroupIndex(0));

  // Remove another mesh group from scene.
  dst_scene.RemoveMeshGroup(draco::MeshGroupIndex(0));
  ASSERT_EQ(dst_scene.NumMeshGroups(), 0);
  ASSERT_EQ(dst_scene.GetNode(draco::SceneNodeIndex(0))->GetMeshGroupIndex(),
            draco::kInvalidMeshGroupIndex);
  ASSERT_EQ(dst_scene.GetNode(draco::SceneNodeIndex(2))->GetMeshGroupIndex(),
            draco::kInvalidMeshGroupIndex);
  ASSERT_EQ(dst_scene.GetNode(draco::SceneNodeIndex(4))->GetMeshGroupIndex(),
            draco::kInvalidMeshGroupIndex);
}

void CheckMeshMaterials(const draco::Scene &scene,
                        const std::vector<int> &expected_material_indices) {
  ASSERT_EQ(scene.NumMeshes(), expected_material_indices.size());
  std::vector<int> scene_material_indices;
  for (draco::MeshGroupIndex i(0); i < scene.NumMeshGroups(); i++) {
    const auto mg = scene.GetMeshGroup(i);
    for (int mi = 0; mi < mg->NumMaterialIndices(); ++mi) {
      scene_material_indices.push_back(mg->GetMaterialIndex(mi));
    }
  }
  ASSERT_EQ(scene_material_indices, expected_material_indices);
}

TEST(SceneTest, TestRemoveMaterial) {
  // Test that materials can be removed from a scene.
  auto src_scene_ptr =
      draco::ReadSceneFromTestFile("CesiumMilkTruck/glTF/CesiumMilkTruck.gltf");
  ASSERT_NE(src_scene_ptr, nullptr);
  const draco::Scene &src_scene = *src_scene_ptr;
  ASSERT_EQ(src_scene.GetMaterialLibrary().NumMaterials(), 4);
  CheckMeshMaterials(src_scene, {0, 1, 2, 3});

  // Copy scene.
  draco::Scene dst_scene;
  dst_scene.Copy(src_scene);

  // Check that referenced material cannot be removed from the scene.
  ASSERT_FALSE(dst_scene.RemoveMaterial(2).ok());

  // Copy scene again, since failed material removal corrupts the scene.
  dst_scene.Copy(src_scene);

  // Remove base mesh from scene. Material at index 2 becomes unreferenced.
  DRACO_ASSERT_OK(dst_scene.RemoveMesh(draco::MeshIndex(2)));
  ASSERT_EQ(dst_scene.GetMaterialLibrary().NumMaterials(), 4);
  CheckMeshMaterials(dst_scene, {0, 1, 3});

  // Check that unreferenced material can be removed from the scene.
  DRACO_ASSERT_OK(dst_scene.RemoveMaterial(2));
  ASSERT_EQ(dst_scene.GetMaterialLibrary().NumMaterials(), 3);
  CheckMeshMaterials(dst_scene, {0, 1, 2});

  // Check that material cannot be removed when material index is out of range.
  ASSERT_FALSE(dst_scene.RemoveMaterial(-1).ok());
  ASSERT_FALSE(dst_scene.RemoveMaterial(3).ok());
}

#endif  // DRACO_TRANSCODER_SUPPORTED

}  // namespace
