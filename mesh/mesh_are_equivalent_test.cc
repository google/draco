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
#include "mesh/mesh_are_equivalent.h"

#include <sstream>

#include "core/draco_test_base.h"
#include "core/draco_test_utils.h"
#include "io/mesh_io.h"
#include "io/obj_decoder.h"
#include "mesh/mesh.h"

namespace draco {

class MeshAreEquivalentTest : public ::testing::Test {
 protected:
  std::unique_ptr<Mesh> DecodeObj(const std::string &file_name) const {
    const std::string path = GetTestFileFullPath(file_name);
    std::unique_ptr<Mesh> mesh(new Mesh());
    ObjDecoder decoder;
    if (!decoder.DecodeFromFile(path, mesh.get()))
      return nullptr;
    return mesh;
  }
};

TEST_F(MeshAreEquivalentTest, TestOnIndenticalMesh) {
  const std::string file_name = "test_nm.obj";
  const std::unique_ptr<Mesh> mesh(DecodeObj(file_name));
  ASSERT_NE(mesh, nullptr) << "Failed to load test model." << file_name;
  MeshAreEquivalent equiv;
  ASSERT_TRUE(equiv(*mesh, *mesh));
}

TEST_F(MeshAreEquivalentTest, TestPermutedOneFace) {
  const std::string file_name_0 = "one_face_123.obj";
  const std::string file_name_1 = "one_face_312.obj";
  const std::string file_name_2 = "one_face_321.obj";
  const std::unique_ptr<Mesh> mesh_0(DecodeObj(file_name_0));
  const std::unique_ptr<Mesh> mesh_1(DecodeObj(file_name_1));
  const std::unique_ptr<Mesh> mesh_2(DecodeObj(file_name_2));
  ASSERT_NE(mesh_0, nullptr) << "Failed to load test model." << file_name_0;
  ASSERT_NE(mesh_1, nullptr) << "Failed to load test model." << file_name_1;
  ASSERT_NE(mesh_2, nullptr) << "Failed to load test model." << file_name_2;
  MeshAreEquivalent equiv;
  ASSERT_TRUE(equiv(*mesh_0, *mesh_0));
  ASSERT_TRUE(equiv(*mesh_0, *mesh_1));   // Face rotated.
  ASSERT_FALSE(equiv(*mesh_0, *mesh_2));  // Face inverted.
}

TEST_F(MeshAreEquivalentTest, TestPermutedTwoFaces) {
  const std::string file_name_0 = "two_faces_123.obj";
  const std::string file_name_1 = "two_faces_312.obj";
  const std::unique_ptr<Mesh> mesh_0(DecodeObj(file_name_0));
  const std::unique_ptr<Mesh> mesh_1(DecodeObj(file_name_1));
  ASSERT_NE(mesh_0, nullptr) << "Failed to load test model." << file_name_0;
  ASSERT_NE(mesh_1, nullptr) << "Failed to load test model." << file_name_1;
  MeshAreEquivalent equiv;
  ASSERT_TRUE(equiv(*mesh_0, *mesh_0));
  ASSERT_TRUE(equiv(*mesh_1, *mesh_1));
  ASSERT_TRUE(equiv(*mesh_0, *mesh_1));
}

//
TEST_F(MeshAreEquivalentTest, TestPermutedThreeFaces) {
  const std::string file_name_0 = "three_faces_123.obj";
  const std::string file_name_1 = "three_faces_312.obj";
  const std::unique_ptr<Mesh> mesh_0(DecodeObj(file_name_0));
  const std::unique_ptr<Mesh> mesh_1(DecodeObj(file_name_1));
  ASSERT_NE(mesh_0, nullptr) << "Failed to load test model." << file_name_0;
  ASSERT_NE(mesh_1, nullptr) << "Failed to load test model." << file_name_1;
  MeshAreEquivalent equiv;
  ASSERT_TRUE(equiv(*mesh_0, *mesh_0));
  ASSERT_TRUE(equiv(*mesh_1, *mesh_1));
  ASSERT_TRUE(equiv(*mesh_0, *mesh_1));
}

// This test checks that the edgebreaker algorithm does not change the mesh up
// to the order of faces and vertices.
TEST_F(MeshAreEquivalentTest, TestOnBigMesh) {
  const std::string file_name = "test_nm.obj";
  const std::unique_ptr<Mesh> mesh0(DecodeObj(file_name));
  ASSERT_NE(mesh0, nullptr) << "Failed to load test model." << file_name;

  std::unique_ptr<Mesh> mesh1;
  std::stringstream ss;
  WriteMeshIntoStream(mesh0.get(), ss, MESH_EDGEBREAKER_ENCODING);
  ReadMeshFromStream(&mesh1, ss);
  ASSERT_TRUE(ss.good()) << "Mesh IO failed.";

  MeshAreEquivalent equiv;
  ASSERT_TRUE(equiv(*mesh0, *mesh0));
  ASSERT_TRUE(equiv(*mesh1, *mesh1));
  ASSERT_TRUE(equiv(*mesh0, *mesh1));
}

}  // namespace draco
