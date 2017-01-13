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

#include "mesh/mesh.h"
#include "core/draco_test_base.h"

namespace draco {

class MeshTest : public ::testing::Test {};

TEST_F(MeshTest, CtorTest) {
  // This test verifies that Mesh Ctor does the job.
  const Mesh mesh;
  ASSERT_EQ(mesh.num_points(), 0u);
  ASSERT_EQ(mesh.GetNamedAttributeId(GeometryAttribute::POSITION), -1);
  ASSERT_EQ(mesh.GetNamedAttribute(GeometryAttribute::POSITION), nullptr);
  ASSERT_EQ(mesh.num_attributes(), 0);
  ASSERT_EQ(mesh.num_faces(), 0);
}

TEST_F(MeshTest, GenTinyMesh) {
  // TODO(hemmer): create a simple mesh builder class to facilitate testing.
  // This test checks properties of a tiny Mesh, i.e., initialized by hand.

  // TODO(hemmer): interface makes it impossible to do further testing
  // Builder functions are all protected, no access to the mesh from the
  // outside.
  // Mesh::Face f1 {{1,2,3}};
  // Mesh::Face f2 {{3, 4, 5}};
  // MeshBuilder builder;
  // builder.Start(2);
  // builder.SetNumVertices(6);
}

}  // namespace draco
