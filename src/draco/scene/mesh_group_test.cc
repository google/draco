// Copyright 2020 The Draco Authors.
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
#include "draco/scene/mesh_group.h"

#include "draco/core/draco_test_base.h"
#include "draco/core/draco_test_utils.h"
#include "draco/scene/scene_indices.h"

namespace {

#ifdef DRACO_TRANSCODER_SUPPORTED

using draco::MeshGroup;
using draco::MeshIndex;

TEST(MeshGroupTest, TestRemoveMeshIndexWithNoOccurrences) {
  // Test that no meshes are removed from mesh group when removing a mesh that
  // is not in the mesh group.

  // Create test mesh group.
  MeshGroup mesh_group;
  mesh_group.AddMeshIndex(MeshIndex(1));
  mesh_group.AddMeshIndex(MeshIndex(3));

  // Try to remove mesh that is not in the mesh group.
  mesh_group.RemoveMeshIndex(MeshIndex(2));

  // Check result.
  ASSERT_EQ(mesh_group.NumMeshIndices(), 2);
  ASSERT_EQ(mesh_group.GetMeshIndex(0), MeshIndex(1));
  ASSERT_EQ(mesh_group.GetMeshIndex(1), MeshIndex(3));
  ASSERT_EQ(mesh_group.NumMaterialIndices(), 0);
}

TEST(MeshGroupTest, TestRemoveMeshIndexWithSingleEnrtyWithMaterial) {
  // Test the only mesh can be removed from mesh group.

  // Create test mesh group.
  MeshGroup mesh_group;
  mesh_group.AddMeshIndex(MeshIndex(7));
  mesh_group.AddMaterialIndex(70);

  // Remove a mesh.
  mesh_group.RemoveMeshIndex(MeshIndex(7));

  // Check result.
  ASSERT_EQ(mesh_group.NumMeshIndices(), 0);
  ASSERT_EQ(mesh_group.NumMaterialIndices(), 0);
}

TEST(MeshGroupTest, TestRemoveMeshIndexWithOneOccurrence) {
  // Test a mesh can be removed from mesh group.

  // Create test mesh group.
  MeshGroup mesh_group;
  mesh_group.AddMeshIndex(MeshIndex(1));
  mesh_group.AddMeshIndex(MeshIndex(3));
  mesh_group.AddMeshIndex(MeshIndex(5));
  mesh_group.AddMeshIndex(MeshIndex(7));

  // Remove a mesh.
  mesh_group.RemoveMeshIndex(MeshIndex(3));

  // Check result.
  ASSERT_EQ(mesh_group.NumMeshIndices(), 3);
  ASSERT_EQ(mesh_group.GetMeshIndex(0), MeshIndex(1));
  ASSERT_EQ(mesh_group.GetMeshIndex(1), MeshIndex(5));
  ASSERT_EQ(mesh_group.GetMeshIndex(2), MeshIndex(7));
  ASSERT_EQ(mesh_group.NumMaterialIndices(), 0);
}

TEST(MeshGroupTest, TestRemoveMeshIndexWithThreeOccurrencesWithMaterials) {
  // Test that multiple meshes can be removed from a mesh group.

  // Create test mesh group.
  MeshGroup mesh_group;
  mesh_group.AddMeshIndex(MeshIndex(1));
  mesh_group.AddMeshIndex(MeshIndex(3));
  mesh_group.AddMeshIndex(MeshIndex(5));
  mesh_group.AddMeshIndex(MeshIndex(1));
  mesh_group.AddMeshIndex(MeshIndex(7));
  mesh_group.AddMeshIndex(MeshIndex(1));
  mesh_group.AddMaterialIndex(10);
  mesh_group.AddMaterialIndex(30);
  mesh_group.AddMaterialIndex(50);
  mesh_group.AddMaterialIndex(10);
  mesh_group.AddMaterialIndex(70);
  mesh_group.AddMaterialIndex(10);

  // Remove a mesh.
  mesh_group.RemoveMeshIndex(MeshIndex(1));

  // Check result.
  ASSERT_EQ(mesh_group.NumMeshIndices(), 3);
  ASSERT_EQ(mesh_group.GetMeshIndex(0), MeshIndex(3));
  ASSERT_EQ(mesh_group.GetMeshIndex(1), MeshIndex(5));
  ASSERT_EQ(mesh_group.GetMeshIndex(2), MeshIndex(7));
  ASSERT_EQ(mesh_group.NumMaterialIndices(), 3);
  ASSERT_EQ(mesh_group.GetMaterialIndex(0), 30);
  ASSERT_EQ(mesh_group.GetMaterialIndex(1), 50);
  ASSERT_EQ(mesh_group.GetMaterialIndex(2), 70);
}

TEST(MeshGroupTest, TestCopy) {
  // Create test mesh group.
  MeshGroup mesh_group;
  mesh_group.SetName("Mesh-1-3-5-7");
  mesh_group.AddMeshIndex(MeshIndex(1));
  mesh_group.AddMeshIndex(MeshIndex(3));
  mesh_group.AddMeshIndex(MeshIndex(5));
  mesh_group.AddMeshIndex(MeshIndex(7));
  mesh_group.AddMaterialIndex(10);
  mesh_group.AddMaterialIndex(30);
  mesh_group.AddMaterialIndex(50);
  mesh_group.AddMaterialIndex(70);

  // Verify source MeshGroup.
  ASSERT_EQ(mesh_group.GetName(), "Mesh-1-3-5-7");
  ASSERT_EQ(mesh_group.NumMeshIndices(), 4);
  ASSERT_EQ(mesh_group.GetMeshIndex(0), MeshIndex(1));
  ASSERT_EQ(mesh_group.GetMeshIndex(1), MeshIndex(3));
  ASSERT_EQ(mesh_group.GetMeshIndex(2), MeshIndex(5));
  ASSERT_EQ(mesh_group.GetMeshIndex(3), MeshIndex(7));
  ASSERT_EQ(mesh_group.NumMaterialIndices(), 4);
  ASSERT_EQ(mesh_group.GetMaterialIndex(0), 10);
  ASSERT_EQ(mesh_group.GetMaterialIndex(1), 30);
  ASSERT_EQ(mesh_group.GetMaterialIndex(2), 50);
  ASSERT_EQ(mesh_group.GetMaterialIndex(3), 70);

  MeshGroup copy_mesh_group;
  copy_mesh_group.Copy(mesh_group);

  // Verify Copy worked.
  ASSERT_EQ(mesh_group.GetName(), copy_mesh_group.GetName());
  ASSERT_EQ(mesh_group.NumMeshIndices(), copy_mesh_group.NumMeshIndices());
  ASSERT_EQ(mesh_group.GetMeshIndex(0), copy_mesh_group.GetMeshIndex(0));
  ASSERT_EQ(mesh_group.GetMeshIndex(1), copy_mesh_group.GetMeshIndex(1));
  ASSERT_EQ(mesh_group.GetMeshIndex(2), copy_mesh_group.GetMeshIndex(2));
  ASSERT_EQ(mesh_group.GetMeshIndex(3), copy_mesh_group.GetMeshIndex(3));
  ASSERT_EQ(mesh_group.NumMaterialIndices(),
            copy_mesh_group.NumMaterialIndices());
  ASSERT_EQ(mesh_group.GetMaterialIndex(0),
            copy_mesh_group.GetMaterialIndex(0));
  ASSERT_EQ(mesh_group.GetMaterialIndex(1),
            copy_mesh_group.GetMaterialIndex(1));
  ASSERT_EQ(mesh_group.GetMaterialIndex(2),
            copy_mesh_group.GetMaterialIndex(2));
  ASSERT_EQ(mesh_group.GetMaterialIndex(3),
            copy_mesh_group.GetMaterialIndex(3));
}

#endif  // DRACO_TRANSCODER_SUPPORTED

}  // namespace
