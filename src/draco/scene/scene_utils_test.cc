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

#include <string>
#include <utility>

#ifdef DRACO_TRANSCODER_SUPPORTED
#include "draco/core/bounding_box.h"
#include "draco/core/draco_test_base.h"
#include "draco/core/draco_test_utils.h"
#include "draco/io/texture_io.h"
#include "draco/metadata/property_table.h"
#include "draco/metadata/structural_metadata.h"
#include "draco/scene/scene_indices.h"

namespace {

using draco::MeshIndex;
using draco::MeshInstanceIndex;

void AssertMatrixNear(const Eigen::Matrix4d& a, const Eigen::Matrix4d& b,
                      float tolerance) {
  Eigen::Matrix4d diff = a - b;
  ASSERT_NEAR(diff.norm(), 0.f, tolerance) << a << " vs " << b;
}

// TODO(fgalligan): Re-factor this code with gltf_encoder_test.
void CompareScenes(const draco::Scene* scene0, const draco::Scene* scene1) {
  ASSERT_EQ(scene0->NumMeshGroups(), scene1->NumMeshGroups());
  ASSERT_EQ(scene0->NumMeshes(), scene1->NumMeshes());
  ASSERT_EQ(scene0->GetMaterialLibrary().NumMaterials(),
            scene1->GetMaterialLibrary().NumMaterials());
  ASSERT_EQ(scene0->NumAnimations(), scene1->NumAnimations());
  ASSERT_EQ(scene0->NumSkins(), scene1->NumSkins());
  for (draco::AnimationIndex i(0); i < scene0->NumAnimations(); ++i) {
    const draco::Animation* const animation0 = scene0->GetAnimation(i);
    const draco::Animation* const animation1 = scene1->GetAnimation(i);
    ASSERT_NE(animation0, nullptr);
    ASSERT_NE(animation1, nullptr);
    ASSERT_EQ(animation0->NumSamplers(), animation1->NumSamplers());
    ASSERT_EQ(animation0->NumChannels(), animation1->NumChannels());
    ASSERT_EQ(animation0->NumNodeAnimationData(),
              animation1->NumNodeAnimationData());
  }
}

TEST(SceneUtilsTest, TestComputeAllInstances) {
  // Tests that we can compute all instances in an input scene along with their
  // transformations.

  auto scene =
      draco::ReadSceneFromTestFile("CesiumMilkTruck/glTF/CesiumMilkTruck.gltf");
  ASSERT_NE(scene, nullptr);
  ASSERT_EQ(scene->NumMeshes(), 4);

  // Compute mesh instances.
  const auto instances = draco::SceneUtils::ComputeAllInstances(*scene);
  ASSERT_EQ(instances.size(), 5);

  // Check base mesh indices.
  ASSERT_EQ(instances[MeshInstanceIndex(0)].mesh_index, 0);
  ASSERT_EQ(instances[MeshInstanceIndex(1)].mesh_index, 1);
  ASSERT_EQ(instances[MeshInstanceIndex(2)].mesh_index, 2);
  ASSERT_EQ(instances[MeshInstanceIndex(3)].mesh_index, 3);
  ASSERT_EQ(instances[MeshInstanceIndex(4)].mesh_index, 3);

  // Check scene node indices.
  ASSERT_EQ(instances[MeshInstanceIndex(0)].scene_node_index, 0);
  ASSERT_EQ(instances[MeshInstanceIndex(1)].scene_node_index, 0);
  ASSERT_EQ(instances[MeshInstanceIndex(2)].scene_node_index, 0);
  ASSERT_EQ(instances[MeshInstanceIndex(3)].scene_node_index, 4);
  ASSERT_EQ(instances[MeshInstanceIndex(4)].scene_node_index, 2);

  // Check indices of meshes in mesh group.
  ASSERT_EQ(instances[MeshInstanceIndex(0)].mesh_group_mesh_index, 0);
  ASSERT_EQ(instances[MeshInstanceIndex(1)].mesh_group_mesh_index, 1);
  ASSERT_EQ(instances[MeshInstanceIndex(2)].mesh_group_mesh_index, 2);
  ASSERT_EQ(instances[MeshInstanceIndex(3)].mesh_group_mesh_index, 0);
  ASSERT_EQ(instances[MeshInstanceIndex(4)].mesh_group_mesh_index, 0);

  // The first three instances should have identity transformation.
  for (MeshInstanceIndex i(0); i < 3; ++i) {
    AssertMatrixNear(instances[i].transform, Eigen::Matrix4d::Identity(),
                     1e-6f);
  }

  // Fourth and fifth instances are transformed.
  Eigen::Matrix4d expected_transform = Eigen::Matrix4d::Identity();
  // Expected translation.
  expected_transform(0, 3) = -1.352329969406128;
  expected_transform(1, 3) = 0.4277220070362091;
  expected_transform(2, 3) = -2.98022992950564e-8;

  // Expected rotation.
  Eigen::Matrix4d expected_rotation = Eigen::Matrix4d::Identity();
  expected_rotation.block<3, 3>(0, 0) =
      Eigen::Quaterniond(-0.9960774183273317, -0.0, -0.0, 0.08848590403795243)
          .normalized()
          .toRotationMatrix();
  expected_transform = expected_transform * expected_rotation;

  AssertMatrixNear(instances[MeshInstanceIndex(3)].transform,
                   expected_transform, 1e-6f);

  // Last instance differs only in the translation part in X axis.
  expected_transform(0, 3) = 1.432669997215271;

  AssertMatrixNear(instances[MeshInstanceIndex(4)].transform,
                   expected_transform, 1e-6f);
}

TEST(SceneUtilsTest, TestComputeInstanceFromRootNode) {
  // Tests that we can compute all instances from a root node of a scene.
  // This should result in the same instances all the ComputeAllInstances().

  auto scene =
      draco::ReadSceneFromTestFile("CesiumMilkTruck/glTF/CesiumMilkTruck.gltf");
  const auto node_instances = draco::SceneUtils::ComputeAllInstancesFromNode(
      *scene, scene->GetRootNodeIndex(0));
  const auto scene_instances = draco::SceneUtils::ComputeAllInstances(*scene);
  ASSERT_EQ(node_instances.size(), scene_instances.size());
  for (draco::MeshInstanceIndex i(0); i < node_instances.size(); ++i) {
    ASSERT_EQ(node_instances[i].scene_node_index,
              scene_instances[i].scene_node_index);
    ASSERT_EQ(node_instances[i].mesh_index, scene_instances[i].mesh_index);
    ASSERT_EQ(node_instances[i].mesh_group_mesh_index,
              scene_instances[i].mesh_group_mesh_index);
    ASSERT_EQ(node_instances[i].transform, scene_instances[i].transform);
  }
}

TEST(SceneUtilsTest, TestComputeInstanceFromChildNode) {
  // Tests that we can compute all instances from a child node of a scene.
  auto scene =
      draco::ReadSceneFromTestFile("CesiumMilkTruck/glTF/CesiumMilkTruck.gltf");
  const auto node_instances = draco::SceneUtils::ComputeAllInstancesFromNode(
      *scene, draco::SceneNodeIndex(1));

  // There should be only one instance in this node chain.
  ASSERT_EQ(node_instances.size(), 1);

  // clang-format off
  AssertMatrixNear(node_instances[draco::MeshInstanceIndex(0)].transform,
            Eigen::Matrix4d{
              { 0.98434,  0.176278, 0, 1.43267},
              {-0.176278, 0.98434,  0, 0.427722},
              {0,         0,        1, -2.98e-8},
              {0,         0,        0, 1}
            }, 1e-6);
  // clang-format on
}

TEST(SceneUtilsTest, TestComputeAllInstancesWithShiftedGeometryRoot) {
  // Tests that we can compute all instances in an input scene along with their
  // transformations. This scene has light and camera nodes before the geometry
  // node.
  auto scene = draco::ReadSceneFromTestFile(
      "SphereWithCircleTexture/sphere_with_circle_texture.gltf");
  ASSERT_NE(scene, nullptr);

  // There is one base mesh.
  ASSERT_EQ(scene->NumMeshes(), 1);

  // There is a single mesh instance.
  const auto instances = draco::SceneUtils::ComputeAllInstances(*scene);
  ASSERT_EQ(instances.size(), 1);
  ASSERT_EQ(instances[MeshInstanceIndex(0)].mesh_index, 0);

  // There is no transformation.
  AssertMatrixNear(instances[MeshInstanceIndex(0)].transform,
                   Eigen::Matrix4d::Identity(), 1e-6);
}

TEST(SceneUtilsTest, TestNumMeshInstances) {
  // Tests that we can compute mesh instance counts for all base meshes in an
  // input scene.

  auto scene =
      draco::ReadSceneFromTestFile("CesiumMilkTruck/glTF/CesiumMilkTruck.gltf");
  ASSERT_NE(scene, nullptr);
  ASSERT_EQ(scene->NumMeshes(), 4);

  const auto num_mesh_instances = draco::SceneUtils::NumMeshInstances(*scene);
  ASSERT_EQ(num_mesh_instances.size(), 4);
  ASSERT_EQ(num_mesh_instances[draco::MeshIndex(0)], 1);
  ASSERT_EQ(num_mesh_instances[draco::MeshIndex(1)], 1);
  ASSERT_EQ(num_mesh_instances[draco::MeshIndex(2)], 1);
  ASSERT_EQ(num_mesh_instances[draco::MeshIndex(3)], 2);
}

TEST(SceneUtilsTest, TestNumFacesOnScene) {
  auto scene =
      draco::ReadSceneFromTestFile("CesiumMilkTruck/glTF/CesiumMilkTruck.gltf");
  ASSERT_NE(scene, nullptr);
  ASSERT_EQ(draco::SceneUtils::NumFacesOnBaseMeshes(*scene), 2856);
  ASSERT_EQ(draco::SceneUtils::NumFacesOnInstancedMeshes(*scene), 3624);
}

TEST(SceneUtilsTest, TestNumPointsOnScene) {
  auto scene =
      draco::ReadSceneFromTestFile("CesiumMilkTruck/glTF/CesiumMilkTruck.gltf");
  ASSERT_NE(scene, nullptr);
  ASSERT_EQ(draco::SceneUtils::NumPointsOnBaseMeshes(*scene), 2978);
  ASSERT_EQ(draco::SceneUtils::NumPointsOnInstancedMeshes(*scene), 3564);
}

TEST(SceneUtilsTest, TestNumPositionsOnScene) {
  auto scene =
      draco::ReadSceneFromTestFile("CesiumMilkTruck/glTF/CesiumMilkTruck.gltf");
  ASSERT_NE(scene, nullptr);
  ASSERT_EQ(draco::SceneUtils::NumAttEntriesOnBaseMeshes(
                *scene, draco::GeometryAttribute::POSITION),
            1572);
  ASSERT_EQ(draco::SceneUtils::NumAttEntriesOnInstancedMeshes(
                *scene, draco::GeometryAttribute::POSITION),
            1960);
}

TEST(SceneUtilsTest, TestNumNormalsOnScene) {
  auto scene =
      draco::ReadSceneFromTestFile("CesiumMilkTruck/glTF/CesiumMilkTruck.gltf");
  ASSERT_NE(scene, nullptr);
  ASSERT_EQ(draco::SceneUtils::NumAttEntriesOnBaseMeshes(
                *scene, draco::GeometryAttribute::NORMAL),
            1252);
  ASSERT_EQ(draco::SceneUtils::NumAttEntriesOnInstancedMeshes(
                *scene, draco::GeometryAttribute::NORMAL),
            1612);
}

TEST(SceneUtilsTest, TestNumColorsOnScene) {
  auto scene =
      draco::ReadSceneFromTestFile("CesiumMilkTruck/glTF/CesiumMilkTruck.gltf");
  ASSERT_NE(scene, nullptr);
  ASSERT_EQ(draco::SceneUtils::NumAttEntriesOnBaseMeshes(
                *scene, draco::GeometryAttribute::COLOR),
            0);
  ASSERT_EQ(draco::SceneUtils::NumAttEntriesOnInstancedMeshes(
                *scene, draco::GeometryAttribute::COLOR),
            0);
}

TEST(SceneUtilsTest, TestComputeBoundingBox) {
  auto scene =
      draco::ReadSceneFromTestFile("CesiumMilkTruck/glTF/CesiumMilkTruck.gltf");
  ASSERT_NE(scene, nullptr);
  const draco::BoundingBox bbox = draco::SceneUtils::ComputeBoundingBox(*scene);
  const draco::Vector3f min_point = bbox.GetMinPoint();
  const draco::Vector3f max_point = bbox.GetMaxPoint();
  constexpr float tolerance = 1e-4f;
  EXPECT_NEAR(min_point[0], -2.43091, tolerance);
  EXPECT_NEAR(min_point[1], +0.00145, tolerance);
  EXPECT_NEAR(min_point[2], -1.39600, tolerance);
  EXPECT_NEAR(max_point[0], +2.43800, tolerance);
  EXPECT_NEAR(max_point[1], +2.58437, tolerance);
  EXPECT_NEAR(max_point[2], +1.39600, tolerance);
}

TEST(SceneUtilsTest, TestComputeMeshInstanceBoundingBox) {
  auto scene = draco::ReadSceneFromTestFile(
      "SphereWithCircleTexture/sphere_with_circle_texture.gltf");
  ASSERT_NE(scene, nullptr);
  const draco::BoundingBox scene_bbox =
      draco::SceneUtils::ComputeBoundingBox(*scene);
  const auto instances = draco::SceneUtils::ComputeAllInstances(*scene);
  ASSERT_EQ(instances.size(), 1);
  const draco::BoundingBox mesh_bbox =
      draco::SceneUtils::ComputeMeshInstanceBoundingBox(
          *scene, instances[draco::MeshInstanceIndex(0)]);
  ASSERT_EQ(scene_bbox.GetMinPoint(), mesh_bbox.GetMinPoint());
  ASSERT_EQ(scene_bbox.GetMaxPoint(), mesh_bbox.GetMaxPoint());
}

TEST(SceneUtilsTest, TestMeshToSceneZeroMaterials) {
  const std::string filename = "cube_att.obj";
  std::unique_ptr<draco::Mesh> mesh = draco::ReadMeshFromTestFile(filename);
  ASSERT_NE(mesh, nullptr);
  ASSERT_EQ(mesh->GetMaterialLibrary().NumMaterials(), 0);
  mesh->SetName("mesh_name");

  DRACO_ASSIGN_OR_ASSERT(const std::unique_ptr<draco::Scene> scene_from_mesh,
                         draco::SceneUtils::MeshToScene(std::move(mesh)));
  ASSERT_NE(scene_from_mesh, nullptr);
  ASSERT_EQ(scene_from_mesh->NumMeshes(), 1);
  ASSERT_EQ(scene_from_mesh->GetMaterialLibrary().NumMaterials(), 1);
  ASSERT_EQ(scene_from_mesh->NumMeshGroups(), 1);
  const draco::MeshGroup* const mesh_group =
      scene_from_mesh->GetMeshGroup(draco::MeshGroupIndex(0));
  ASSERT_EQ(mesh_group->NumMeshInstances(), 1);
  ASSERT_EQ(mesh_group->GetName(), "mesh_name");
}

TEST(SceneUtilsTest, TestMeshToSceneOneMaterial) {
  const std::string filename =
      "SphereWithCircleTexture/sphere_with_circle_texture.gltf";
  auto scene = draco::ReadSceneFromTestFile(filename);
  ASSERT_NE(scene, nullptr);
  ASSERT_EQ(scene->GetMaterialLibrary().NumMaterials(), 1);

  std::unique_ptr<draco::Mesh> mesh = draco::ReadMeshFromTestFile(filename);
  ASSERT_NE(mesh, nullptr);
  ASSERT_EQ(mesh->GetMaterialLibrary().NumMaterials(), 1);

  DRACO_ASSIGN_OR_ASSERT(const std::unique_ptr<draco::Scene> scene_from_mesh,
                         draco::SceneUtils::MeshToScene(std::move(mesh)));
  ASSERT_NE(scene_from_mesh, nullptr);
  ASSERT_EQ(scene_from_mesh->NumMeshes(), 1);
  ASSERT_EQ(scene_from_mesh->GetMaterialLibrary().NumMaterials(), 1);
  ASSERT_EQ(scene_from_mesh->NumMeshGroups(), 1);
  const draco::MeshGroup* const mesh_group =
      scene_from_mesh->GetMeshGroup(draco::MeshGroupIndex(0));
  ASSERT_EQ(mesh_group->NumMeshInstances(), 1);

  CompareScenes(scene.get(), scene_from_mesh.get());
}

TEST(SceneUtilsTest, TestMeshToSceneMultipleMaterials) {
  const std::string filename = "CesiumMilkTruck/glTF/CesiumMilkTruck.gltf";
  auto scene = draco::ReadSceneFromTestFile(filename);
  ASSERT_NE(scene, nullptr);

  std::unique_ptr<draco::Mesh> mesh = draco::ReadMeshFromTestFile(filename);
  ASSERT_NE(mesh, nullptr);
  ASSERT_EQ(mesh->GetMaterialLibrary().NumMaterials(), 4);

  DRACO_ASSIGN_OR_ASSERT(const std::unique_ptr<draco::Scene> scene_from_mesh,
                         draco::SceneUtils::MeshToScene(std::move(mesh)));
  ASSERT_NE(scene_from_mesh, nullptr);
  ASSERT_EQ(scene_from_mesh->NumMeshes(), 4);
  ASSERT_EQ(scene_from_mesh->GetMaterialLibrary().NumMaterials(), 4);
  ASSERT_EQ(scene_from_mesh->NumMeshGroups(), 1);
  const draco::MeshGroup* const mesh_group =
      scene_from_mesh->GetMeshGroup(draco::MeshGroupIndex(0));
  ASSERT_EQ(mesh_group->NumMeshInstances(), 4);

  // Unfortunately we can't CompareScenes(scene.get(), scene_from_mesh.get()),
  // because scene has two mesh groups and scene_from_mesh has only one.
}

TEST(SceneUtilsTest, TestMeshToSceneMultipleMeshFeatures) {
  const std::string filename = "BoxesMeta/glTF/BoxesMeta.gltf";
  std::unique_ptr<draco::Scene> scene = draco::ReadSceneFromTestFile(filename);
  ASSERT_NE(scene, nullptr);
  std::unique_ptr<draco::Mesh> mesh = draco::ReadMeshFromTestFile(filename);
  ASSERT_NE(mesh, nullptr);
  ASSERT_EQ(mesh->GetMaterialLibrary().NumMaterials(), 2);
  ASSERT_EQ(mesh->NumMeshFeatures(), 5);

  DRACO_ASSIGN_OR_ASSERT(const std::unique_ptr<draco::Scene> scene_from_mesh,
                         draco::SceneUtils::MeshToScene(std::move(mesh)));
  ASSERT_NE(scene_from_mesh, nullptr);
  ASSERT_EQ(scene_from_mesh->NumMeshes(), 2);
  ASSERT_EQ(scene_from_mesh->GetMaterialLibrary().NumMaterials(), 2);
  ASSERT_EQ(scene_from_mesh->NumMeshGroups(), 1);
  const draco::MeshGroup* const mesh_group =
      scene_from_mesh->GetMeshGroup(draco::MeshGroupIndex(0));
  ASSERT_EQ(mesh_group->NumMeshInstances(), 2);

  // Meshes of the new scene should have the same properties as meshes loaded
  // directly into |scene|.
  for (draco::MeshIndex mi(0); mi < scene->NumMeshes(); ++mi) {
    ASSERT_EQ(scene->GetMesh(mi).NumMeshFeatures(),
              scene_from_mesh->GetMesh(mi).NumMeshFeatures());
    for (draco::MeshFeaturesIndex mfi(0);
         mfi < scene->GetMesh(mi).NumMeshFeatures(); ++mfi) {
      const auto& scene_mf = scene->GetMesh(mi).GetMeshFeatures(mfi);
      const auto& scene_from_mesh_mf =
          scene_from_mesh->GetMesh(mi).GetMeshFeatures(mfi);
      const int att_index_0 = scene_mf.GetAttributeIndex();
      const int att_index_1 = scene_from_mesh_mf.GetAttributeIndex();
      if (att_index_0 == -1) {
        ASSERT_EQ(att_index_0, att_index_1);
      } else {
        ASSERT_EQ(scene->GetMesh(mi).attribute(att_index_0)->name(),
                  scene_from_mesh->GetMesh(mi).attribute(att_index_1)->name());
        ASSERT_EQ(scene->GetMesh(mi).attribute(att_index_0)->size(),
                  scene_from_mesh->GetMesh(mi).attribute(att_index_1)->size());
      }

      ASSERT_EQ(scene_mf.GetPropertyTableIndex(),
                scene_from_mesh_mf.GetPropertyTableIndex());
      ASSERT_EQ(scene_mf.GetLabel(), scene_from_mesh_mf.GetLabel());
      ASSERT_EQ(scene_mf.GetNullFeatureId(),
                scene_from_mesh_mf.GetNullFeatureId());
      ASSERT_EQ(scene_mf.GetFeatureCount(),
                scene_from_mesh_mf.GetFeatureCount());
      ASSERT_EQ(scene_mf.GetTextureChannels(),
                scene_from_mesh_mf.GetTextureChannels());
      ASSERT_EQ(scene_mf.GetTextureMap().texture() != nullptr,
                scene_from_mesh_mf.GetTextureMap().texture() != nullptr);
    }
  }
}

TEST(SceneUtilsTest, TestMeshToSceneMeshFeaturesWithAttributes) {
  // Tests that converting a mesh into scene properly updates mesh features
  // attribute indices.
  auto mesh =
      draco::ReadMeshFromTestFile("CesiumMilkTruck/glTF/CesiumMilkTruck.gltf");
  ASSERT_NE(mesh, nullptr);

  // Add a new dummy mesh features and mesh features attribute to the mesh.
  std::unique_ptr<draco::PointAttribute> mf_att(new draco::PointAttribute());
  mf_att->Init(draco::GeometryAttribute::GENERIC, 1, draco::DT_FLOAT32, false,
               mesh->num_points());
  const int mf_att_id = mesh->AddAttribute(std::move(mf_att));
  std::unique_ptr<draco::MeshFeatures> mf(new draco::MeshFeatures());
  mf->SetAttributeIndex(mf_att_id);
  mesh->AddMeshFeatures(std::move(mf));

  // Convert the mesh into a scene.
  DRACO_ASSIGN_OR_ASSERT(const std::unique_ptr<draco::Scene> scene_from_mesh,
                         draco::SceneUtils::MeshToScene(std::move(mesh)));
  ASSERT_NE(scene_from_mesh, nullptr);

  // Ensure the attribute indices on the meshes from scene are decremented by
  // one because the material attribute was removed.
  ASSERT_EQ(scene_from_mesh->NumMeshes(), 4);
  for (draco::MeshIndex mi(0); mi < scene_from_mesh->NumMeshes(); ++mi) {
    for (draco::MeshFeaturesIndex mfi(0);
         mfi < scene_from_mesh->GetMesh(mi).NumMeshFeatures(); ++mfi) {
      const auto& mf = scene_from_mesh->GetMesh(mi).GetMeshFeatures(mfi);
      ASSERT_EQ(mf.GetAttributeIndex(), mf_att_id - 1);
    }
  }
}

TEST(SceneUtilsTest, TestMeshToSceneStructuralMetadata) {
  const std::string filename = "cube_att.obj";
  std::unique_ptr<draco::Mesh> mesh = draco::ReadMeshFromTestFile(filename);
  ASSERT_NE(mesh, nullptr);

  // Setting a sample schema to:
  // {
  //   "classes": []
  // }
  draco::StructuralMetadataSchema sample_schema;
  auto& classes_json = sample_schema.json.SetObjects().emplace_back("classes");
  classes_json.SetArray();

  mesh->GetStructuralMetadata().SetSchema(sample_schema);
  draco::StructuralMetadata mesh_structural_metadata;
  mesh_structural_metadata.Copy(mesh->GetStructuralMetadata());
  ASSERT_FALSE(mesh_structural_metadata.GetSchema().Empty());

  DRACO_ASSIGN_OR_ASSERT(const std::unique_ptr<draco::Scene> scene_from_mesh,
                         draco::SceneUtils::MeshToScene(std::move(mesh)));
  ASSERT_NE(scene_from_mesh, nullptr);
  ASSERT_EQ(scene_from_mesh->GetStructuralMetadata(), mesh_structural_metadata);
}

TEST(SceneUtilsTest, TestInstantiateMeshWithIdentityTransformation) {
  auto scene =
      draco::ReadSceneFromTestFile("CesiumMilkTruck/glTF/CesiumMilkTruck.gltf");
  ASSERT_NE(scene, nullptr);

  // Compute scene mesh instances.
  const auto instances = draco::SceneUtils::ComputeAllInstances(*scene);
  ASSERT_EQ(instances.size(), 5);

  // Check instantiation of mesh with identity transformation.
  const draco::SceneUtils::MeshInstance instance =
      instances[MeshInstanceIndex(0)];
  ASSERT_EQ(instance.transform, Eigen::Matrix4d::Identity());

  // Instantiate this mesh instance.
  DRACO_ASSIGN_OR_ASSERT(auto mesh,
                         draco::SceneUtils::InstantiateMesh(*scene, instance));
  const draco::Mesh& base_mesh = scene->GetMesh(instance.mesh_index);

  // Check that bounding box of the instanced mesh is same as box of base mesh.
  const draco::BoundingBox instanced_bbox = mesh->ComputeBoundingBox();
  const draco::BoundingBox base_bbox = base_mesh.ComputeBoundingBox();
  ASSERT_EQ(instanced_bbox.GetMinPoint()[0], base_bbox.GetMinPoint()[0]);
  ASSERT_EQ(instanced_bbox.GetMinPoint()[1], base_bbox.GetMinPoint()[1]);
  ASSERT_EQ(instanced_bbox.GetMinPoint()[2], base_bbox.GetMinPoint()[2]);
  ASSERT_EQ(instanced_bbox.GetMaxPoint()[0], base_bbox.GetMaxPoint()[0]);
  ASSERT_EQ(instanced_bbox.GetMaxPoint()[1], base_bbox.GetMaxPoint()[1]);
  ASSERT_EQ(instanced_bbox.GetMaxPoint()[2], base_bbox.GetMaxPoint()[2]);
}

TEST(SceneUtilsTest, TestInstantiateMesh) {
  auto scene =
      draco::ReadSceneFromTestFile("CesiumMilkTruck/glTF/CesiumMilkTruck.gltf");
  ASSERT_NE(scene, nullptr);

  // Compute scene mesh instances.
  const auto instances = draco::SceneUtils::ComputeAllInstances(*scene);
  ASSERT_EQ(instances.size(), 5);

  // Check instantiation of mesh with identity transformation.
  const draco::SceneUtils::MeshInstance instance =
      instances[MeshInstanceIndex(3)];
  ASSERT_NE(instance.transform, Eigen::Matrix4d::Identity());

  // Instantiate this mesh instance.
  DRACO_ASSIGN_OR_ASSERT(auto mesh,
                         draco::SceneUtils::InstantiateMesh(*scene, instance));
  const draco::Mesh& base_mesh = scene->GetMesh(instance.mesh_index);

  // Check bounding box of the base mesh.
  constexpr float tolerance = 1e-4f;
  const draco::BoundingBox base_bbox = base_mesh.ComputeBoundingBox();
  EXPECT_NEAR(base_bbox.GetMinPoint()[0], -0.42780, tolerance);
  EXPECT_NEAR(base_bbox.GetMinPoint()[1], -0.42780, tolerance);
  EXPECT_NEAR(base_bbox.GetMinPoint()[2], -1.05800, tolerance);
  EXPECT_NEAR(base_bbox.GetMaxPoint()[0], +0.42780, tolerance);
  EXPECT_NEAR(base_bbox.GetMaxPoint()[1], +0.42780, tolerance);
  EXPECT_NEAR(base_bbox.GetMaxPoint()[2], +1.05800, tolerance);

  // Check bounding box of the instanced mesh. It should differ.
  const draco::BoundingBox instanced_bbox = mesh->ComputeBoundingBox();
  EXPECT_NEAR(instanced_bbox.GetMinPoint()[0], -1.77860, tolerance);
  EXPECT_NEAR(instanced_bbox.GetMinPoint()[1], +0.00145, tolerance);
  EXPECT_NEAR(instanced_bbox.GetMinPoint()[2], -1.05800, tolerance);
  EXPECT_NEAR(instanced_bbox.GetMaxPoint()[0], -0.92606, tolerance);
  EXPECT_NEAR(instanced_bbox.GetMaxPoint()[1], +0.85399, tolerance);
  EXPECT_NEAR(instanced_bbox.GetMaxPoint()[2], +1.05800, tolerance);
}

TEST(SceneUtilsTest, TestInstantiateMeshWithInvalidMeshIndex) {
  // Instantiating a mesh with an index that is not present in the scene must
  // return an error instead of accessing the out-of-bounds mesh.
  draco::Scene scene;
  const draco::SceneUtils::MeshInstance invalid_instance = {
      draco::MeshIndex(0), draco::SceneNodeIndex(0), 0,
      Eigen::Matrix4d::Identity()};
  const auto result =
      draco::SceneUtils::InstantiateMesh(scene, invalid_instance);
  EXPECT_FALSE(result.ok());
}

TEST(SceneUtilsTest, TestCleanupEmptyMeshGroup) {
  auto scene =
      draco::ReadSceneFromTestFile("CesiumMilkTruck/glTF/CesiumMilkTruck.gltf");
  ASSERT_NE(scene, nullptr);
  ASSERT_EQ(scene->NumMeshes(), 4);
  ASSERT_EQ(scene->NumMeshGroups(), 2);
  ASSERT_EQ(draco::SceneUtils::ComputeAllInstances(*scene).size(), 5);
  ASSERT_EQ(scene->GetNode(draco::SceneNodeIndex(0))->GetMeshGroupIndex(),
            draco::MeshGroupIndex(0));

  // Invalidate references to the three truck body parts in mesh group.
  draco::MeshGroup& mesh_group = *scene->GetMeshGroup(draco::MeshGroupIndex(0));
  mesh_group.SetMeshInstance(0, {draco::kInvalidMeshIndex, 0});
  mesh_group.SetMeshInstance(1, {draco::kInvalidMeshIndex, 0});
  mesh_group.SetMeshInstance(2, {draco::kInvalidMeshIndex, 0});

  // Cleanup scene.
  draco::SceneUtils::Cleanup(scene.get());

  // Check cleaned up scene.
  ASSERT_EQ(scene->NumMeshes(), 1);
  ASSERT_EQ(scene->NumMeshGroups(), 1);
  ASSERT_EQ(draco::SceneUtils::ComputeAllInstances(*scene).size(), 2);
  ASSERT_EQ(scene->GetNode(draco::SceneNodeIndex(0))->GetMeshGroupIndex(),
            draco::kInvalidMeshGroupIndex);
}

TEST(SceneUtilsTest, TestCleanupUnreferencedMeshGroup) {
  auto scene =
      draco::ReadSceneFromTestFile("CesiumMilkTruck/glTF/CesiumMilkTruck.gltf");
  ASSERT_NE(scene, nullptr);
  ASSERT_EQ(scene->NumMeshes(), 4);
  ASSERT_EQ(scene->NumMeshGroups(), 2);
  ASSERT_EQ(draco::SceneUtils::ComputeAllInstances(*scene).size(), 5);

  // Invalidate references to truck axle mesh group.
  scene->GetNode(draco::SceneNodeIndex(2))
      ->SetMeshGroupIndex(draco::kInvalidMeshGroupIndex);
  scene->GetNode(draco::SceneNodeIndex(4))
      ->SetMeshGroupIndex(draco::kInvalidMeshGroupIndex);

  // Cleanup scene.
  draco::SceneUtils::Cleanup(scene.get());

  // Check cleaned up scene.
  ASSERT_EQ(scene->NumMeshes(), 3);
  ASSERT_EQ(scene->NumMeshGroups(), 1);
  ASSERT_EQ(draco::SceneUtils::ComputeAllInstances(*scene).size(), 3);
}

TEST(SceneUtilsTest, TestCleanupInvalidMeshIndex) {
  auto scene =
      draco::ReadSceneFromTestFile("CesiumMilkTruck/glTF/CesiumMilkTruck.gltf");
  ASSERT_NE(scene, nullptr);
  ASSERT_EQ(scene->NumMeshes(), 4);
  ASSERT_EQ(scene->NumMeshGroups(), 2);
  ASSERT_EQ(draco::SceneUtils::ComputeAllInstances(*scene).size(), 5);
  ASSERT_EQ(scene->GetNode(draco::SceneNodeIndex(0))->GetMeshGroupIndex(),
            draco::MeshGroupIndex(0));

  // Invalidate references to two truck body parts in mesh group.
  draco::MeshGroup& mesh_group = *scene->GetMeshGroup(draco::MeshGroupIndex(0));
  ASSERT_EQ(mesh_group.NumMeshInstances(), 3);
  mesh_group.SetMeshInstance(0, {draco::kInvalidMeshIndex, 0});
  mesh_group.SetMeshInstance(2, {draco::kInvalidMeshIndex, 0});

  // Cleanup scene.
  draco::SceneUtils::Cleanup(scene.get());

  // Check cleaned up scene.
  ASSERT_EQ(scene->NumMeshes(), 2);
  ASSERT_EQ(scene->NumMeshGroups(), 2);
  ASSERT_EQ(draco::SceneUtils::ComputeAllInstances(*scene).size(), 3);
  ASSERT_EQ(scene->GetMeshGroup(draco::MeshGroupIndex(0))->NumMeshInstances(),
            1);
}

TEST(SceneUtilsTest, TestCleanupUnusedNodes) {
  auto scene =
      draco::ReadSceneFromTestFile("CesiumMilkTruck/glTF/CesiumMilkTruck.gltf");
  ASSERT_NE(scene, nullptr);
  ASSERT_EQ(scene->NumNodes(), 5);

  draco::SceneUtils::CleanupOptions options;
  options.remove_unused_nodes = true;

  // Delete mesh on node 2 and try to remove unused nodes.
  // Node 2 is connected to node 1 that has no mesh as well. But node 2 is also
  // used in an animation so we don't actually expect anything to be deleted.
  scene->GetNode(draco::SceneNodeIndex(2))
      ->SetMeshGroupIndex(draco::kInvalidMeshGroupIndex);
  draco::SceneUtils::Cleanup(scene.get(), options);

  ASSERT_EQ(scene->NumNodes(), 5);

  // Now remove the animation channel that used the node and try it again. This
  // time, we expect two nodes to be deleted (node 1 and node 2). Node 1 will be
  // deleted because it doesn't contain a mesh and all its children are unused.
  ASSERT_EQ(scene->GetAnimation(draco::AnimationIndex(0))
                ->GetChannel(0)
                ->target_index,
            2);
  // Change the mapped node to node 4 (we can't actually remove channel as of
  // the time this test was written).
  scene->GetAnimation(draco::AnimationIndex(0))->GetChannel(0)->target_index =
      4;

  // Cleanup again.
  draco::SceneUtils::Cleanup(scene.get(), options);
  ASSERT_EQ(scene->NumNodes(), 3);  // Two nodes should be deleted.

  // Ensure all node indices are remapped to the new values.
  for (draco::SceneNodeIndex sni(0); sni < scene->NumNodes(); ++sni) {
    const auto* node = scene->GetNode(sni);
    for (int i = 0; i < node->NumChildren(); ++i) {
      ASSERT_LT(node->Child(i).value(), 3);
    }
    for (int i = 0; i < node->NumParents(); ++i) {
      ASSERT_LT(node->Parent(i).value(), 3);
    }
  }

  // Ensure the animation channels are mapped to the updated node indices (node
  // 4 should be new node 2 because two nodes were removed).
  ASSERT_EQ(scene->GetAnimation(draco::AnimationIndex(0))
                ->GetChannel(0)
                ->target_index,
            2);
}

TEST(SceneUtilsTest, TestDeduplicateMeshGroups) {
  // Input scene has four different mesh groups but only two of them should
  // contain unique set of meshes.
  auto scene =
      draco::ReadSceneFromTestFile("DuplicateMeshes/duplicate_meshes.gltf");
  ASSERT_NE(scene, nullptr);
  ASSERT_EQ(scene->NumMeshes(), 1);
  ASSERT_EQ(scene->NumMeshGroups(), 4);
  ASSERT_EQ(draco::SceneUtils::ComputeAllInstances(*scene).size(), 7);

  draco::SceneUtils::DeduplicateMeshGroups(scene.get());

  // Check deduplicated scene.
  ASSERT_EQ(scene->NumMeshes(), 1);
  ASSERT_EQ(scene->NumMeshGroups(), 2);
  ASSERT_EQ(draco::SceneUtils::ComputeAllInstances(*scene).size(), 7);
}

TEST(SceneUtilsTest, TestCleanupUnusedTexCoordsNoTextures) {
  // The glTF file has two tex coords that are unused because the materials do
  // not reference any textures.
  auto scene = draco::ReadSceneFromTestFile("UnusedTexCoords/NoTextures.gltf");
  ASSERT_NE(scene, nullptr);
  ASSERT_EQ(scene->GetMesh(draco::MeshIndex(0))
                .NumNamedAttributes(draco::GeometryAttribute::TEX_COORD),
            2);

  // Cleanup scene and check that unused UV are not removed by default.
  draco::SceneUtils::Cleanup(scene.get());
  ASSERT_EQ(scene->GetMesh(draco::MeshIndex(0))
                .NumNamedAttributes(draco::GeometryAttribute::TEX_COORD),
            2);

  // Cleanup scene and check that unused UV are removed when requested.
  draco::SceneUtils::CleanupOptions options;
  options.remove_unused_tex_coords = true;
  draco::SceneUtils::Cleanup(scene.get(), options);
  ASSERT_EQ(scene->GetMesh(draco::MeshIndex(0))
                .NumNamedAttributes(draco::GeometryAttribute::TEX_COORD),
            0);
}

TEST(SceneUtilsTest, TestCleanupUnusedTexCoords0NoReferences) {
  auto scene = draco::ReadSceneFromTestFile(
      "UnusedTexCoords/TexCoord0InvalidTexCoord1Valid.gltf");
  ASSERT_NE(scene, nullptr);
  typedef draco::GeometryAttribute Att;

  draco::Mesh& mesh = scene->GetMesh(draco::MeshIndex(0));
  ASSERT_EQ(mesh.NumNamedAttributes(Att::TEX_COORD), 2);
  ASSERT_EQ(mesh.GetNamedAttribute(Att::TEX_COORD, 0)->size(), 14);
  ASSERT_EQ(mesh.GetNamedAttribute(Att::TEX_COORD, 1)->size(), 4);
  auto& ml = scene->GetMaterialLibrary();
  ASSERT_EQ(ml.NumMaterials(), 1);
  ASSERT_EQ(ml.GetMaterial(0)->NumTextureMaps(), 1);
  ASSERT_EQ(ml.GetMaterial(0)->GetTextureMapByIndex(0)->tex_coord_index(), 1);

  // Cleanup unused texture coordinate attributes.
  draco::SceneUtils::CleanupOptions options;
  options.remove_unused_tex_coords = true;
  draco::SceneUtils::Cleanup(scene.get(), options);

  // Check that the unreferenced attribute was removed.
  ASSERT_EQ(mesh.NumNamedAttributes(Att::TEX_COORD), 1);
  ASSERT_EQ(mesh.GetNamedAttribute(Att::TEX_COORD, 0)->size(), 4);
  ASSERT_EQ(ml.NumMaterials(), 1);
  ASSERT_EQ(ml.GetMaterial(0)->NumTextureMaps(), 1);
  ASSERT_EQ(ml.GetMaterial(0)->GetTextureMapByIndex(0)->tex_coord_index(), 0);
}

TEST(SceneUtilsTest, TestCleanupUnusedTexCoords1NoReferences) {
  auto scene = draco::ReadSceneFromTestFile(
      "UnusedTexCoords/TexCoord0ValidTexCoord1Invalid.gltf");
  ASSERT_NE(scene, nullptr);
  typedef draco::GeometryAttribute Att;

  draco::Mesh& mesh = scene->GetMesh(draco::MeshIndex(0));
  ASSERT_EQ(mesh.NumNamedAttributes(Att::TEX_COORD), 2);
  ASSERT_EQ(mesh.GetNamedAttribute(Att::TEX_COORD, 0)->size(), 14);
  ASSERT_EQ(mesh.GetNamedAttribute(Att::TEX_COORD, 1)->size(), 4);
  auto& ml = scene->GetMaterialLibrary();
  ASSERT_EQ(ml.NumMaterials(), 1);
  ASSERT_EQ(ml.GetMaterial(0)->NumTextureMaps(), 1);
  ASSERT_EQ(ml.GetMaterial(0)->GetTextureMapByIndex(0)->tex_coord_index(), 0);

  // Cleanup unused texture coordinate attributes.
  draco::SceneUtils::CleanupOptions options;
  options.remove_unused_tex_coords = true;
  draco::SceneUtils::Cleanup(scene.get(), options);

  // Check that the unreferenced attribute was removed.
  ASSERT_EQ(mesh.NumNamedAttributes(Att::TEX_COORD), 1);
  ASSERT_EQ(mesh.GetNamedAttribute(Att::TEX_COORD, 0)->size(), 14);
  ASSERT_EQ(ml.NumMaterials(), 1);
  ASSERT_EQ(ml.GetMaterial(0)->NumTextureMaps(), 1);
  ASSERT_EQ(ml.GetMaterial(0)->GetTextureMapByIndex(0)->tex_coord_index(), 0);
}

TEST(SceneUtilsTest, TestCleanupUnusedTexCoordsWithMeshFeatures) {
  // Test that texture coordinates used by MeshFeatures are not cleaned up.
  auto scene = std::make_unique<draco::Scene>();
  auto mesh = std::make_unique<draco::Mesh>();

  // Add POSITION attribute.
  auto pos_att = std::make_unique<draco::PointAttribute>();
  pos_att->Init(draco::GeometryAttribute::POSITION, 3, draco::DT_FLOAT32, false,
                4);
  mesh->AddAttribute(std::move(pos_att));

  // Add 2 texture coordinate attributes.
  auto tex_coord_0 = std::make_unique<draco::PointAttribute>();
  tex_coord_0->Init(draco::GeometryAttribute::TEX_COORD, 2, draco::DT_FLOAT32,
                    false, 4);
  mesh->AddAttribute(std::move(tex_coord_0));

  auto tex_coord_1 = std::make_unique<draco::PointAttribute>();
  tex_coord_1->Init(draco::GeometryAttribute::TEX_COORD, 2, draco::DT_FLOAT32,
                    false, 4);
  mesh->AddAttribute(std::move(tex_coord_1));

  ASSERT_EQ(mesh->NumNamedAttributes(draco::GeometryAttribute::TEX_COORD), 2);

  // Add a texture-mapped MeshFeatures referencing TEX_COORD 1.
  auto mf = std::make_unique<draco::MeshFeatures>();
  mf->SetTextureMap(nullptr, 1);  // tex_coord_index = 1
  mesh->AddMeshFeatures(std::move(mf));

  // Add mesh to scene.
  const draco::MeshIndex mesh_index = scene->AddMesh(std::move(mesh));

  // Add a dummy mesh group node to make it referenced in scene.
  const draco::MeshGroupIndex mg_index = scene->AddMeshGroup();
  draco::MeshGroup& mesh_group = *scene->GetMeshGroup(mg_index);
  mesh_group.AddMeshInstance({mesh_index, -1});

  const draco::SceneNodeIndex node_index = scene->AddNode();
  draco::SceneNode& node = *scene->GetNode(node_index);
  node.SetMeshGroupIndex(mg_index);

  // Cleanup unused texture coordinates.
  draco::SceneUtils::CleanupOptions options;
  options.remove_unused_tex_coords = true;
  draco::SceneUtils::Cleanup(scene.get(), options);

  // Verify that:
  // 1. One TEX_COORD attribute remains.
  // 2. The remaining attribute's tex_coord_index in MeshFeatures's texture map
  //    has decremented from 1 to 0.
  const draco::Mesh& cleaned_mesh = scene->GetMesh(mesh_index);
  ASSERT_EQ(
      cleaned_mesh.NumNamedAttributes(draco::GeometryAttribute::TEX_COORD), 1);
  ASSERT_EQ(cleaned_mesh.NumMeshFeatures(), 1);
  const draco::MeshFeatures& cleaned_mf =
      cleaned_mesh.GetMeshFeatures(draco::MeshFeaturesIndex(0));
  ASSERT_EQ(cleaned_mf.GetTextureMap().tex_coord_index(), 0);
}

TEST(SceneUtilsTest, TestCleanupUnusedTexCoordsWithPropertyTextures) {
  // Test that texture coordinates used by PropertyTextures are not cleaned up.
  auto scene = std::make_unique<draco::Scene>();
  auto mesh = std::make_unique<draco::Mesh>();

  // Add POSITION attribute.
  auto pos_att = std::make_unique<draco::PointAttribute>();
  pos_att->Init(draco::GeometryAttribute::POSITION, 3, draco::DT_FLOAT32, false,
                4);
  mesh->AddAttribute(std::move(pos_att));

  // Add 2 texture coordinate attributes.
  auto tex_coord_0 = std::make_unique<draco::PointAttribute>();
  tex_coord_0->Init(draco::GeometryAttribute::TEX_COORD, 2, draco::DT_FLOAT32,
                    false, 4);
  mesh->AddAttribute(std::move(tex_coord_0));

  auto tex_coord_1 = std::make_unique<draco::PointAttribute>();
  tex_coord_1->Init(draco::GeometryAttribute::TEX_COORD, 2, draco::DT_FLOAT32,
                    false, 4);
  mesh->AddAttribute(std::move(tex_coord_1));

  ASSERT_EQ(mesh->NumNamedAttributes(draco::GeometryAttribute::TEX_COORD), 2);

  // Set up structural metadata property texture in the scene.
  draco::StructuralMetadataSchema schema;
  schema.json.SetString("Data");
  scene->GetStructuralMetadata().SetSchema(schema);

  auto property_texture = std::make_unique<draco::PropertyTexture>();
  property_texture->SetClass("Motion");
  auto property = std::make_unique<draco::PropertyTexture::Property>();
  property->SetName("Magnitude");
  const std::vector<int> channels = {0};
  property->SetTextureChannels(channels);
  property->SetTextureMap(nullptr, 1);  // tex_coord_index = 1
  property_texture->AddProperty(std::move(property));
  scene->GetStructuralMetadata().AddPropertyTexture(
      std::move(property_texture));

  // Mesh uses the property texture.
  mesh->AddPropertyTexturesIndex(0);

  // Add mesh to scene.
  const draco::MeshIndex mesh_index = scene->AddMesh(std::move(mesh));

  // Add a dummy mesh group node to make it referenced in scene.
  const draco::MeshGroupIndex mg_index = scene->AddMeshGroup();
  draco::MeshGroup& mesh_group = *scene->GetMeshGroup(mg_index);
  mesh_group.AddMeshInstance({mesh_index, -1});

  const draco::SceneNodeIndex node_index = scene->AddNode();
  draco::SceneNode& node = *scene->GetNode(node_index);
  node.SetMeshGroupIndex(mg_index);

  // Cleanup unused texture coordinates.
  draco::SceneUtils::CleanupOptions options;
  options.remove_unused_tex_coords = true;
  draco::SceneUtils::Cleanup(scene.get(), options);

  // Verify that:
  // 1. One TEX_COORD attribute remains.
  // 2. The remaining attribute's tex_coord_index in PropertyTexture
  //    has decremented from 1 to 0.
  const draco::Mesh& cleaned_mesh = scene->GetMesh(mesh_index);
  ASSERT_EQ(
      cleaned_mesh.NumNamedAttributes(draco::GeometryAttribute::TEX_COORD), 1);
  ASSERT_EQ(cleaned_mesh.NumPropertyTexturesIndices(), 1);
  ASSERT_EQ(cleaned_mesh.GetPropertyTexturesIndex(0), 0);

  const auto& property_texture_result =
      scene->GetStructuralMetadata().GetPropertyTexture(0);
  ASSERT_EQ(property_texture_result.NumProperties(), 1);
  const auto& property_result = property_texture_result.GetProperty(0);
  ASSERT_EQ(property_result.GetTextureMap().tex_coord_index(), 0);
}

TEST(SceneUtilsTest, TestCleanupUnusedTexCoordsWithSharedPropertyTextures) {
  // Setup a scene with two meshes that share the same PropertyTexture:
  // - Mesh 0 has 2 TEX_COORDs.
  //   Attribute 0: unused.
  //   Attribute 1: used by shared PropertyTexture (referenced at index 1).
  // - Mesh 1 has 2 TEX_COORDs.
  //   Attribute 0: unused.
  //   Attribute 1: used by shared PropertyTexture (referenced at index 1).
  //
  // Because they share the same PropertyTexture, we must NOT clean up their
  // texture coordinates, because we cannot safely decrement the shared
  // PropertyTexture.
  auto scene = std::make_unique<draco::Scene>();

  // Set up structural metadata property texture in the scene.
  draco::StructuralMetadataSchema schema;
  schema.json.SetString("Data");
  scene->GetStructuralMetadata().SetSchema(schema);

  auto property_texture = std::make_unique<draco::PropertyTexture>();
  property_texture->SetClass("Motion");
  auto property = std::make_unique<draco::PropertyTexture::Property>();
  property->SetName("Magnitude");
  const std::vector<int> channels = {0};
  property->SetTextureChannels(channels);
  property->SetTextureMap(nullptr, 1);  // tex_coord_index = 1
  property_texture->AddProperty(std::move(property));
  scene->GetStructuralMetadata().AddPropertyTexture(
      std::move(property_texture));

  // Build Mesh 0.
  auto mesh_0 = std::make_unique<draco::Mesh>();
  auto pos_0 = std::make_unique<draco::PointAttribute>();
  pos_0->Init(draco::GeometryAttribute::POSITION, 3, draco::DT_FLOAT32, false,
              4);
  mesh_0->AddAttribute(std::move(pos_0));

  auto tc0_0 = std::make_unique<draco::PointAttribute>();
  tc0_0->Init(draco::GeometryAttribute::TEX_COORD, 2, draco::DT_FLOAT32, false,
              4);
  mesh_0->AddAttribute(std::move(tc0_0));

  auto tc0_1 = std::make_unique<draco::PointAttribute>();
  tc0_1->Init(draco::GeometryAttribute::TEX_COORD, 2, draco::DT_FLOAT32, false,
              4);
  mesh_0->AddAttribute(std::move(tc0_1));

  mesh_0->AddPropertyTexturesIndex(0);  // Uses PT 0
  const draco::MeshIndex m0_idx = scene->AddMesh(std::move(mesh_0));

  // Build Mesh 1.
  auto mesh_1 = std::make_unique<draco::Mesh>();
  auto pos_1 = std::make_unique<draco::PointAttribute>();
  pos_1->Init(draco::GeometryAttribute::POSITION, 3, draco::DT_FLOAT32, false,
              4);
  mesh_1->AddAttribute(std::move(pos_1));

  auto tc1_0 = std::make_unique<draco::PointAttribute>();
  tc1_0->Init(draco::GeometryAttribute::TEX_COORD, 2, draco::DT_FLOAT32, false,
              4);
  mesh_1->AddAttribute(std::move(tc1_0));

  auto tc1_1 = std::make_unique<draco::PointAttribute>();
  tc1_1->Init(draco::GeometryAttribute::TEX_COORD, 2, draco::DT_FLOAT32, false,
              4);
  mesh_1->AddAttribute(std::move(tc1_1));

  mesh_1->AddPropertyTexturesIndex(0);  // Uses PT 0 (shared!)
  const draco::MeshIndex m1_idx = scene->AddMesh(std::move(mesh_1));

  // Setup instances and nodes so meshes are referenced.
  const draco::MeshGroupIndex mg_index = scene->AddMeshGroup();
  draco::MeshGroup& mesh_group = *scene->GetMeshGroup(mg_index);
  mesh_group.AddMeshInstance({m0_idx, -1});
  mesh_group.AddMeshInstance({m1_idx, -1});

  const draco::SceneNodeIndex node_index = scene->AddNode();
  draco::SceneNode& node = *scene->GetNode(node_index);
  node.SetMeshGroupIndex(mg_index);

  // Cleanup.
  draco::SceneUtils::CleanupOptions options;
  options.remove_unused_tex_coords = true;
  draco::SceneUtils::Cleanup(scene.get(), options);

  // Verify that:
  // Both meshes keep their 2 TEX_COORD attributes, because they share the PT.
  const draco::Mesh& cleaned_m0 = scene->GetMesh(m0_idx);
  const draco::Mesh& cleaned_m1 = scene->GetMesh(m1_idx);

  ASSERT_EQ(cleaned_m0.NumNamedAttributes(draco::GeometryAttribute::TEX_COORD),
            2);
  ASSERT_EQ(cleaned_m1.NumNamedAttributes(draco::GeometryAttribute::TEX_COORD),
            2);

  // PropertyTexture's tex_coord_index is still 1.
  const auto& property_texture_result =
      scene->GetStructuralMetadata().GetPropertyTexture(0);
  const auto& property_result = property_texture_result.GetProperty(0);
  ASSERT_EQ(property_result.GetTextureMap().tex_coord_index(), 1);
}

TEST(SceneUtilsTest, TestCleanupUnusedTexCoordsPreservesWrappingAndFiltering) {
  // Test that cleanup preserves custom wrapping and filtering modes when
  // shifting.
  auto scene = std::make_unique<draco::Scene>();
  auto mesh = std::make_unique<draco::Mesh>();

  // Add POSITION attribute.
  auto pos_att = std::make_unique<draco::PointAttribute>();
  pos_att->Init(draco::GeometryAttribute::POSITION, 3, draco::DT_FLOAT32, false,
                4);
  mesh->AddAttribute(std::move(pos_att));

  // Add 2 texture coordinate attributes.
  auto tex_coord_0 = std::make_unique<draco::PointAttribute>();
  tex_coord_0->Init(draco::GeometryAttribute::TEX_COORD, 2, draco::DT_FLOAT32,
                    false, 4);
  mesh->AddAttribute(std::move(tex_coord_0));

  auto tex_coord_1 = std::make_unique<draco::PointAttribute>();
  tex_coord_1->Init(draco::GeometryAttribute::TEX_COORD, 2, draco::DT_FLOAT32,
                    false, 4);
  mesh->AddAttribute(std::move(tex_coord_1));

  ASSERT_EQ(mesh->NumNamedAttributes(draco::GeometryAttribute::TEX_COORD), 2);

  // Set up a material that uses TEX_COORD 1, but with custom wrapping (REPEAT)
  // and filtering (LINEAR).
  auto material = scene->GetMaterialLibrary().MutableMaterial(0);
  auto texture = std::make_unique<draco::Texture>();

  // Set up the texture library and push the texture so material owns it.
  const int texture_index =
      scene->GetMaterialLibrary().MutableTextureLibrary().PushTexture(
          std::move(texture));
  draco::Texture* texture_ptr =
      scene->GetMaterialLibrary().MutableTextureLibrary().GetTexture(
          texture_index);

  draco::TextureMap::WrappingMode wrapping_mode(draco::TextureMap::REPEAT);
  DRACO_ASSERT_OK(material->SetTextureMap(
      texture_ptr, draco::TextureMap::COLOR, wrapping_mode,
      draco::TextureMap::LINEAR, draco::TextureMap::LINEAR, 1));

  // Add mesh to scene.
  const draco::MeshIndex mesh_index = scene->AddMesh(std::move(mesh));

  // Add a dummy mesh group node to make it referenced in scene.
  const draco::MeshGroupIndex mg_index = scene->AddMeshGroup();
  draco::MeshGroup& mesh_group = *scene->GetMeshGroup(mg_index);
  mesh_group.AddMeshInstance({mesh_index, 0});

  const draco::SceneNodeIndex node_index = scene->AddNode();
  draco::SceneNode& node = *scene->GetNode(node_index);
  node.SetMeshGroupIndex(mg_index);

  // Cleanup unused texture coordinates.
  draco::SceneUtils::CleanupOptions options;
  options.remove_unused_tex_coords = true;
  draco::SceneUtils::Cleanup(scene.get(), options);

  // Verify that:
  // 1. One TEX_COORD attribute remains.
  // 2. The remaining attribute index in the material has shifted from 1 to 0.
  // 3. The wrapping mode (REPEAT) is preserved!
  // 4. The min/mag filters (LINEAR) are preserved!
  const draco::Mesh& cleaned_mesh = scene->GetMesh(mesh_index);
  ASSERT_EQ(
      cleaned_mesh.NumNamedAttributes(draco::GeometryAttribute::TEX_COORD), 1);

  const auto& material_library = scene->GetMaterialLibrary();
  ASSERT_EQ(material_library.NumMaterials(), 1);
  const auto& material_result = material_library.GetMaterial(0);
  ASSERT_EQ(material_result->NumTextureMaps(), 1);
  const auto* texture_map_result = material_result->GetTextureMapByIndex(0);

  // Check index shifting:
  ASSERT_EQ(texture_map_result->tex_coord_index(), 0);

  // Check preservation:
  ASSERT_EQ(texture_map_result->wrapping_mode().s, draco::TextureMap::REPEAT);
  ASSERT_EQ(texture_map_result->wrapping_mode().t, draco::TextureMap::REPEAT);
  ASSERT_EQ(texture_map_result->min_filter(), draco::TextureMap::LINEAR);
  ASSERT_EQ(texture_map_result->mag_filter(), draco::TextureMap::LINEAR);
}

TEST(SceneUtilsTest, TestComputeGlobalNodeTransform) {
  // Tests that we can compute global transformation of scene nodes.

  auto scene = draco::ReadSceneFromTestFile("simple_skin.gltf");
  ASSERT_NE(scene, nullptr);
  ASSERT_EQ(scene->NumNodes(), 3);

  // Compute and check global node transforms.
  constexpr float kTolerance = 1e-6;
  // clang-format off
  AssertMatrixNear(draco::SceneUtils::ComputeGlobalNodeTransform(
                       *scene, draco::SceneNodeIndex(0)),
                   Eigen::Matrix4d::Identity(),
                   kTolerance);
  AssertMatrixNear(draco::SceneUtils::ComputeGlobalNodeTransform(
                       *scene, draco::SceneNodeIndex(1)),
                   Eigen::Matrix4d{{1.0, 0.0, 0.0, 0.0},
                                   {0.0, 1.0, 0.0, 1.0},
                                   {0.0, 0.0, 1.0, 0.0},
                                   {0.0, 0.0, 0.0, 1.0}},
                   kTolerance);
  AssertMatrixNear(draco::SceneUtils::ComputeGlobalNodeTransform(
                       *scene, draco::SceneNodeIndex(2)),
                   Eigen::Matrix4d{{1.0, 0.0, 0.0, 0.0},
                                   {0.0, 1.0, 0.0, 1.0},
                                   {0.0, 0.0, 1.0, 0.0},
                                   {0.0, 0.0, 0.0, 1.0}},
                   kTolerance);
  // clang-format on
}

TEST(SceneUtilsTest, TestIsDracoCompressionEnabled) {
  // Tests that we can determine whether any of the scene meshes have geometry
  // compression enabled.
  const std::string file = "CesiumMilkTruck/glTF/CesiumMilkTruck.gltf";
  auto scene = draco::ReadSceneFromTestFile(file);
  ASSERT_NE(scene, nullptr);
  ASSERT_EQ(scene->NumMeshes(), 4);

  // Check that the scene has geometry compression disabled by default.
  ASSERT_FALSE(draco::SceneUtils::IsDracoCompressionEnabled(*scene));

  // Check that geometry compression can be enabled.
  scene->GetMesh(MeshIndex(2)).SetCompressionEnabled(true);
  ASSERT_TRUE(draco::SceneUtils::IsDracoCompressionEnabled(*scene));
}

TEST(SceneUtilsTest, TestSetDracoCompressionOptions) {
  // Tests that geometry compression settings can be set for all scene meshes.
  const std::string file = "CesiumMilkTruck/glTF/CesiumMilkTruck.gltf";
  auto scene = draco::ReadSceneFromTestFile(file);
  ASSERT_NE(scene, nullptr);
  ASSERT_EQ(scene->NumMeshes(), 4);

  // Check that compression is initially disabled for all scene meshes.
  ASSERT_FALSE(scene->GetMesh(MeshIndex(0)).IsCompressionEnabled());
  ASSERT_FALSE(scene->GetMesh(MeshIndex(1)).IsCompressionEnabled());
  ASSERT_FALSE(scene->GetMesh(MeshIndex(2)).IsCompressionEnabled());
  ASSERT_FALSE(scene->GetMesh(MeshIndex(3)).IsCompressionEnabled());

  // Check that initially all scene meshes have default compression options.
  draco::DracoCompressionOptions defaults;
  ASSERT_EQ(scene->GetMesh(MeshIndex(0)).GetCompressionOptions(), defaults);
  ASSERT_EQ(scene->GetMesh(MeshIndex(1)).GetCompressionOptions(), defaults);
  ASSERT_EQ(scene->GetMesh(MeshIndex(2)).GetCompressionOptions(), defaults);
  ASSERT_EQ(scene->GetMesh(MeshIndex(3)).GetCompressionOptions(), defaults);

  // Check geometry compression options can be set to all scene meshes and that
  // this also enables compression for all scnene meshes.
  draco::DracoCompressionOptions options;
  options.compression_level = 10;
  options.quantization_bits_normal = 12;
  draco::SceneUtils::SetDracoCompressionOptions(&options, scene.get());
  ASSERT_TRUE(scene->GetMesh(MeshIndex(0)).IsCompressionEnabled());
  ASSERT_TRUE(scene->GetMesh(MeshIndex(1)).IsCompressionEnabled());
  ASSERT_TRUE(scene->GetMesh(MeshIndex(2)).IsCompressionEnabled());
  ASSERT_TRUE(scene->GetMesh(MeshIndex(3)).IsCompressionEnabled());
  ASSERT_EQ(scene->GetMesh(MeshIndex(0)).GetCompressionOptions(), options);
  ASSERT_EQ(scene->GetMesh(MeshIndex(1)).GetCompressionOptions(), options);
  ASSERT_EQ(scene->GetMesh(MeshIndex(2)).GetCompressionOptions(), options);
  ASSERT_EQ(scene->GetMesh(MeshIndex(3)).GetCompressionOptions(), options);

  // Check that geometry compression can be disabled for all scene meshes.
  draco::SceneUtils::SetDracoCompressionOptions(nullptr, scene.get());
  ASSERT_FALSE(scene->GetMesh(MeshIndex(0)).IsCompressionEnabled());
  ASSERT_FALSE(scene->GetMesh(MeshIndex(1)).IsCompressionEnabled());
  ASSERT_FALSE(scene->GetMesh(MeshIndex(2)).IsCompressionEnabled());
  ASSERT_FALSE(scene->GetMesh(MeshIndex(3)).IsCompressionEnabled());
}

TEST(SceneUtilsTest, TestFindLargestBaseMeshTransforms) {
  // Tests that FindLargestBaseMeshTransforms() works as expected.
  auto scene =
      draco::ReadSceneFromTestFile("CubeScaledInstances/glTF/cube_att.gltf");
  ASSERT_NE(scene, nullptr);

  // There should be one base mesh with four instances.
  ASSERT_EQ(scene->NumMeshes(), 1);
  ASSERT_EQ(draco::SceneUtils::ComputeAllInstances(*scene).size(), 4);

  const auto transforms =
      draco::SceneUtils::FindLargestBaseMeshTransforms(*scene);

  ASSERT_EQ(transforms.size(), 1);  // One transform for the single base mesh.

  // The largest instance should have a uniform scale 4.
  const draco::MeshIndex mi(0);
  ASSERT_EQ(transforms[mi].diagonal(), Eigen::Vector4d(4, 4, 4, 1));
}

}  // namespace

#endif  // DRACO_TRANSCODER_SUPPORTED
