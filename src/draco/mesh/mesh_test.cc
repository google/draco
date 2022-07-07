// Copyright 2018 The Draco Authors.
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
#include "draco/mesh/mesh.h"

#include <memory>

#include "draco/core/draco_test_base.h"
#include "draco/core/draco_test_utils.h"

#ifdef DRACO_TRANSCODER_SUPPORTED
#include "draco/compression/draco_compression_options.h"
#include "draco/mesh/mesh_are_equivalent.h"
#include "draco/mesh/triangle_soup_mesh_builder.h"
#endif  // DRACO_TRANSCODER_SUPPORTED

namespace {

#ifdef DRACO_TRANSCODER_SUPPORTED
// Tests naming of a mesh.
TEST(MeshTest, MeshName) {
  draco::Mesh mesh;
  ASSERT_TRUE(mesh.GetName().empty());
  mesh.SetName("Bob");
  ASSERT_EQ(mesh.GetName(), "Bob");
}

// Tests copying of a mesh.
TEST(MeshTest, MeshCopy) {
  const std::unique_ptr<draco::Mesh> mesh =
      draco::ReadMeshFromTestFile("cube_att.obj");
  ASSERT_NE(mesh, nullptr);
  draco::Mesh mesh_copy;
  mesh_copy.Copy(*mesh);
  draco::MeshAreEquivalent eq;
  ASSERT_TRUE(eq(*mesh, mesh_copy));
}

// Tests that we can copy a mesh to a different mesh that already contains some
// data.
TEST(MeshTest, MeshCopyToExistingMesh) {
  const std::unique_ptr<draco::Mesh> mesh_0 =
      draco::ReadMeshFromTestFile("cube_att.obj");
  const std::unique_ptr<draco::Mesh> mesh_1 =
      draco::ReadMeshFromTestFile("test_nm.obj");
  ASSERT_NE(mesh_0, nullptr);
  ASSERT_NE(mesh_1, nullptr);
  draco::MeshAreEquivalent eq;
  ASSERT_FALSE(eq(*mesh_0, *mesh_1));

  mesh_1->Copy(*mesh_0);
  ASSERT_TRUE(eq(*mesh_0, *mesh_1));
}

// Tests that we can remove unused materials from a mesh.
TEST(MeshTest, RemoveUnusedMaterials) {
  // Input mesh has 29 materials defined in the source file but only 7 are
  // actually used.
  const std::unique_ptr<draco::Mesh> mesh =
      draco::ReadMeshFromTestFile("mat_test.obj");
  ASSERT_NE(mesh, nullptr);

  const draco::PointAttribute *const mat_att =
      mesh->GetNamedAttribute(draco::GeometryAttribute::MATERIAL);
  ASSERT_NE(mat_att, nullptr);
  ASSERT_EQ(mat_att->size(), 29);

  ASSERT_EQ(mesh->GetMaterialLibrary().NumMaterials(), mat_att->size());

  // Get materials on all faces.
  std::vector<const draco::Material *> face_materials(mesh->num_faces(),
                                                      nullptr);
  for (draco::FaceIndex fi(0); fi < mesh->num_faces(); ++fi) {
    uint32_t mat_index = 0;
    mat_att->GetMappedValue(mesh->face(fi)[0], &mat_index);
    face_materials[fi.value()] =
        mesh->GetMaterialLibrary().GetMaterial(mat_index);
  }

  mesh->RemoveUnusedMaterials();

  ASSERT_EQ(mesh->GetMaterialLibrary().NumMaterials(), 7);

  // Ensure the material attribute contains material indices in the valid range.
  for (draco::AttributeValueIndex avi(0); avi < mat_att->size(); ++avi) {
    uint32_t mat_index = 0;
    mat_att->GetValue(avi, &mat_index);
    ASSERT_LT(mat_index, mesh->GetMaterialLibrary().NumMaterials());
  }

  // Ensure all materials are still the same for all faces.
  for (draco::FaceIndex fi(0); fi < mesh->num_faces(); ++fi) {
    uint32_t mat_index = 0;
    mat_att->GetMappedValue(mesh->face(fi)[0], &mat_index);
    ASSERT_EQ(mesh->GetMaterialLibrary().GetMaterial(mat_index),
              face_materials[fi.value()]);
  }
}

TEST(MeshTest, TestAddNewAttributeWithConnectivity) {
  // Tests that we can add new attributes with arbitrary connectivity to an
  // existing mesh.

  // Create a simple quad. See corner indices of the quad on the figure below:
  //
  //  *-------*
  //  |2\3   5|
  //  |  \    |
  //  |   \   |
  //  |    \  |
  //  |     \4|
  //  |0    1\|
  //  *-------*
  //
  draco::TriangleSoupMeshBuilder mb;
  mb.Start(2);
  mb.AddAttribute(draco::GeometryAttribute::POSITION, 3, draco::DT_FLOAT32);
  mb.SetAttributeValuesForFace(
      0, draco::FaceIndex(0), draco::Vector3f(0, 0, 0).data(),
      draco::Vector3f(1, 0, 0).data(), draco::Vector3f(1, 1, 0).data());
  mb.SetAttributeValuesForFace(
      0, draco::FaceIndex(1), draco::Vector3f(1, 1, 0).data(),
      draco::Vector3f(1, 0, 0).data(), draco::Vector3f(1, 1, 1).data());
  std::unique_ptr<draco::Mesh> mesh = mb.Finalize();
  ASSERT_NE(mesh, nullptr);
  ASSERT_EQ(mesh->num_points(), 4);
  ASSERT_EQ(mesh->GetNamedAttribute(draco::GeometryAttribute::POSITION)->size(),
            4);

  // Create a simple attribute that has a constant value on every corner.
  std::unique_ptr<draco::PointAttribute> pa(new draco::PointAttribute());
  pa->Init(draco::GeometryAttribute::GENERIC, 1 /*One components*/,
           draco::DT_UINT8, false, 1);
  uint8_t val = 10;
  pa->SetAttributeValue(draco::AttributeValueIndex(0), &val);

  // Map all corners to the same value.
  draco::IndexTypeVector<draco::CornerIndex, draco::AttributeValueIndex>
      corner_to_point(6, draco::AttributeValueIndex(0));

  // Adding this attribute to the mesh should not increase the number of points.
  const int new_att_id_0 =
      mesh->AddAttributeWithConnectivity(std::move(pa), corner_to_point);

  ASSERT_EQ(mesh->num_attributes(), 2);
  ASSERT_EQ(mesh->num_points(), 4);

  const draco::PointAttribute *const new_att_0 = mesh->attribute(new_att_id_0);
  ASSERT_NE(new_att_0, nullptr);

  // All points of the mesh should be mapped to the same attribute value.
  for (draco::PointIndex pi(0); pi < mesh->num_points(); ++pi) {
    uint8_t att_val = 0;
    new_att_0->GetMappedValue(pi, &att_val);
    ASSERT_EQ(att_val, 10);
  }

  // Add a new attribute with two values and different connectivity.
  pa = std::unique_ptr<draco::PointAttribute>(new draco::PointAttribute());
  pa->Init(draco::GeometryAttribute::GENERIC, 1 /*One components*/,
           draco::DT_UINT8, false, 2);
  val = 11;
  pa->SetAttributeValue(draco::AttributeValueIndex(0), &val);
  val = 12;
  pa->SetAttributeValue(draco::AttributeValueIndex(1), &val);

  // Map all corners to the value index 0 except for corner 1 that is mapped to
  // value index 1. This should result in a new point being created on either
  // corner 1 or corner 4 (see figure at the beginning of this test).
  corner_to_point.assign(6, draco::AttributeValueIndex(0));
  corner_to_point[draco::CornerIndex(1)] = draco::AttributeValueIndex(1);

  const int new_att_id_1 =
      mesh->AddAttributeWithConnectivity(std::move(pa), corner_to_point);

  ASSERT_EQ(mesh->num_attributes(), 3);

  // One new point should have been created by adding the new attribute.
  ASSERT_EQ(mesh->num_points(), 5);

  const draco::PointAttribute *const new_att_1 = mesh->attribute(new_att_id_1);
  ASSERT_NE(new_att_1, nullptr);
  ASSERT_TRUE(mesh->CornerToPointId(1) == draco::PointIndex(4) ||
              mesh->CornerToPointId(4) == draco::PointIndex(4));

  new_att_1->GetMappedValue(mesh->CornerToPointId(1), &val);
  ASSERT_EQ(val, 12);

  new_att_1->GetMappedValue(mesh->CornerToPointId(4), &val);
  ASSERT_EQ(val, 11);

  // Ensure the attribute values of the remaining attributes are well defined
  // on the new point.
  draco::Vector3f pos;
  mesh->attribute(0)->GetMappedValue(draco::PointIndex(4), &pos[0]);
  ASSERT_EQ(pos, draco::Vector3f(1, 0, 0));

  new_att_0->GetMappedValue(draco::PointIndex(4), &val);
  ASSERT_EQ(val, 10);

  new_att_0->GetMappedValue(mesh->CornerToPointId(1), &val);
  ASSERT_EQ(val, 10);
  new_att_0->GetMappedValue(mesh->CornerToPointId(4), &val);
  ASSERT_EQ(val, 10);
}

TEST(MeshTest, TestAddNewAttributeWithConnectivityWithIsolatedVertices) {
  // Tests that we can add a new attribute with connectivity to a mesh that
  // contains isolated vertices.
  const std::unique_ptr<draco::Mesh> mesh =
      draco::ReadMeshFromTestFile("isolated_vertices.ply");
  ASSERT_NE(mesh, nullptr);
  const draco::PointAttribute *const pos_att =
      mesh->GetNamedAttribute(draco::GeometryAttribute::POSITION);
  ASSERT_NE(pos_att, nullptr);
  ASSERT_TRUE(pos_att->is_mapping_identity());
  ASSERT_EQ(pos_att->size(), 5);
  ASSERT_EQ(mesh->num_points(), 5);
  ASSERT_EQ(mesh->num_faces(), 2);

  // Add a new attribute with two values (one for each face).
  auto pa = std::unique_ptr<draco::PointAttribute>(new draco::PointAttribute());
  pa->Init(draco::GeometryAttribute::GENERIC, 1 /*One component*/,
           draco::DT_UINT8, false, 2);
  uint8_t val = 11;
  pa->SetAttributeValue(draco::AttributeValueIndex(0), &val);
  val = 12;
  pa->SetAttributeValue(draco::AttributeValueIndex(1), &val);

  draco::IndexTypeVector<draco::CornerIndex, draco::AttributeValueIndex>
      corner_to_point(6, draco::AttributeValueIndex(0));
  // All corners on the second face are mapped to the value 1.
  for (draco::CornerIndex ci(3); ci < 6; ++ci) {
    corner_to_point[ci] = draco::AttributeValueIndex(1);
  }

  const draco::PointAttribute *const pa_raw = pa.get();
  mesh->AddAttributeWithConnectivity(std::move(pa), corner_to_point);

  // Two new point should have been added.
  ASSERT_EQ(mesh->num_points(), 7);

  for (draco::PointIndex pi(0); pi < mesh->num_points(); ++pi) {
    ASSERT_NE(pa_raw->mapped_index(pi), draco::kInvalidAttributeValueIndex);
    ASSERT_NE(pos_att->mapped_index(pi), draco::kInvalidAttributeValueIndex);
  }
}

TEST(MeshTest, TestAddPerVertexAttribute) {
  std::unique_ptr<draco::Mesh> mesh =
      draco::ReadMeshFromTestFile("cube_att.obj");

  ASSERT_NE(mesh, nullptr);
  const draco::PointAttribute *const pos_att =
      mesh->GetNamedAttribute(draco::GeometryAttribute::POSITION);
  ASSERT_NE(pos_att, nullptr);

  // The input mesh should have 8 spatial vertices.
  ASSERT_EQ(pos_att->size(), 8);

  // Add a new scalar attribute where each value corresponds to the position
  // value index (vertex).
  std::unique_ptr<draco::PointAttribute> pa(new draco::PointAttribute());
  pa->Init(draco::GeometryAttribute::GENERIC, /* scalar */ 1, draco::DT_FLOAT32,
           false, /* one value per position value */ 8);

  // Set the value for the new attribute.
  for (draco::AttributeValueIndex avi(0); avi < 8; ++avi) {
    const float att_value = avi.value();
    pa->SetAttributeValue(avi, &att_value);
  }

  // Add the attribute to the existing mesh.
  const int new_att_id = mesh->AddPerVertexAttribute(std::move(pa));
  ASSERT_NE(new_att_id, -1);

  // Make sure all the attribute values are set correctly for every point of the
  // mesh.
  for (draco::PointIndex pi(0); pi < mesh->num_points(); ++pi) {
    const draco::AttributeValueIndex pos_avi = pos_att->mapped_index(pi);
    const draco::AttributeValueIndex new_att_avi =
        mesh->attribute(new_att_id)->mapped_index(pi);
    ASSERT_EQ(pos_avi, new_att_avi);

    float new_att_value;
    mesh->attribute(new_att_id)->GetValue(new_att_avi, &new_att_value);
    ASSERT_EQ(new_att_value, new_att_avi.value());
  }
}

TEST(MeshTest, TestRemovalOfIsolatedPoints) {
  const std::unique_ptr<draco::Mesh> mesh =
      draco::ReadMeshFromTestFile("isolated_vertices.ply");

  draco::Mesh mesh_copy;
  mesh_copy.Copy(*mesh);

  ASSERT_EQ(mesh_copy.num_points(), 5);
  mesh_copy.RemoveIsolatedPoints();
  ASSERT_EQ(mesh_copy.num_points(), 4);

  draco::MeshAreEquivalent eq;
  ASSERT_TRUE(eq(*mesh, mesh_copy));
}

TEST(MeshTest, TestCompressionSettings) {
  // Tests compression settings of a mesh.
  const std::unique_ptr<draco::Mesh> mesh =
      draco::ReadMeshFromTestFile("cube_att.obj");
  ASSERT_NE(mesh, nullptr);

  // Check that compression is disabled and compression settings are default.
  ASSERT_FALSE(mesh->IsCompressionEnabled());
  const draco::DracoCompressionOptions default_compression_options;
  ASSERT_EQ(mesh->GetCompressionOptions(), default_compression_options);

  // Check that compression options can be set without enabling compression.
  draco::DracoCompressionOptions compression_options;
  compression_options.quantization_bits_normal = 12;
  mesh->SetCompressionOptions(compression_options);
  ASSERT_EQ(mesh->GetCompressionOptions(), compression_options);
  ASSERT_FALSE(mesh->IsCompressionEnabled());

  // Check that compression can be enabled.
  mesh->SetCompressionEnabled(true);
  ASSERT_TRUE(mesh->IsCompressionEnabled());

  // Check that individual compression options can be updated.
  mesh->GetCompressionOptions().compression_level++;
  mesh->GetCompressionOptions().compression_level--;

  // Check that compression settings can be copied.
  draco::Mesh mesh_copy;
  mesh_copy.Copy(*mesh);
  ASSERT_TRUE(mesh_copy.IsCompressionEnabled());
  ASSERT_EQ(mesh_copy.GetCompressionOptions(), compression_options);
}
#endif  // DRACO_TRANSCODER_SUPPORTED

// Test bounding box.
TEST(MeshTest, TestMeshBoundingBox) {
  const draco::Vector3f max_pt(1, 1, 1);
  const draco::Vector3f min_pt(0, 0, 0);

  const std::unique_ptr<draco::Mesh> mesh =
      draco::ReadMeshFromTestFile("cube_att.obj");
  ASSERT_NE(mesh, nullptr) << "Failed in Loading: "
                           << "cube_att.obj";
  const draco::BoundingBox bounding_box = mesh->ComputeBoundingBox();

  EXPECT_EQ(max_pt[0], bounding_box.GetMaxPoint()[0]);
  EXPECT_EQ(max_pt[1], bounding_box.GetMaxPoint()[1]);
  EXPECT_EQ(max_pt[2], bounding_box.GetMaxPoint()[2]);

  EXPECT_EQ(min_pt[0], bounding_box.GetMinPoint()[0]);
  EXPECT_EQ(min_pt[1], bounding_box.GetMinPoint()[1]);
  EXPECT_EQ(min_pt[2], bounding_box.GetMinPoint()[2]);
}

}  // namespace
