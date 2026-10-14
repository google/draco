// Copyright 2026 The Draco Authors.
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
#include "draco/draco_features.h"

#ifdef DRACO_PMC_SUPPORTED

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "draco/animation/skin.h"
#include "draco/attributes/geometry_attribute.h"
#include "draco/attributes/point_attribute.h"
#include "draco/compression/draco_compression_options.h"
#include "draco/compression/pmc/pmc_draco_mesh_decoder.h"
#include "draco/compression/pmc/pmc_draco_mesh_encoder.h"
#include "draco/compression/pmc/pmc_utils.h"
#include "draco/core/bounding_box.h"
#include "draco/core/decoder_buffer.h"
#include "draco/core/draco_test_utils.h"
#include "draco/core/encoder_buffer.h"
#include "draco/core/status.h"
#include "draco/mesh/mesh.h"
#include "draco/metadata/geometry_metadata.h"
#include "draco/scene/scene.h"
#include "gtest/gtest.h"

namespace draco {

// Compares bounds for each dimension of two bounding boxes.
template <typename T, int D>
void Compare(const BoundingBoxD<T, D>& bbox1, const BoundingBoxD<T, D>& bbox2,
             float tolerance, const std::string& label) {
  for (int i = 0; i < D; ++i) {
    ASSERT_NEAR(bbox1.GetMinPoint()[i], bbox2.GetMinPoint()[i], tolerance)
        << label << " Min Bound " << i;
    ASSERT_NEAR(bbox1.GetMaxPoint()[i], bbox2.GetMaxPoint()[i], tolerance)
        << label << " Max Bound " << i;
  }
}

TEST(PmcDracoMeshEncoderTest, CubeAttributes) {
  auto mesh = draco::ReadMeshFromTestFile("cube_att.obj");
  ASSERT_NE(mesh, nullptr);
  ASSERT_EQ(mesh->num_faces(), 12);
  ASSERT_EQ(mesh->num_points(), 24);

  // Compute input mesh bounding box.
  BoundingBox input_position_bbox = mesh->ComputeBoundingBox();
  BoundingBoxD<float, 2> input_tex_coord_bbox =
      PmcUtils::ComputeBoundingBox<float, 2>(
          *mesh->GetNamedAttribute(GeometryAttribute::TEX_COORD));
  BoundingBox input_normal_bbox = PmcUtils::ComputeBoundingBox<float, 3>(
      *mesh->GetNamedAttribute(GeometryAttribute::NORMAL));

  // Set quantization options.
  DracoCompressionOptions& options = mesh->GetCompressionOptions();
  options.quantization_position.SetQuantizationBits(12);
  options.quantization_bits_tex_coord = 10;
  options.quantization_bits_normal = 8;

  // Encode mesh with PmcDracoMeshEncoder.
  PmcDracoMeshEncoder encoder;
  EncoderBuffer out_buffer;
  ASSERT_TRUE(encoder.Encode(*mesh, &out_buffer).ok());
  ASSERT_GT(out_buffer.size(), 50);

  // Decode mesh with PmcDracoMeshDecoder.
  DecoderBuffer in_buffer;
  in_buffer.Init(out_buffer.data(), out_buffer.size());
  PmcDracoMeshDecoder decoder;
  Mesh decoded_mesh;
  draco::Status status = decoder.Decode(&in_buffer, &decoded_mesh);
  ASSERT_TRUE(status.ok()) << status.error_msg();

  // Check decoded mesh.
  ASSERT_EQ(decoded_mesh.num_faces(), 12);
  ASSERT_EQ(decoded_mesh.num_points(), 24);

  // Compare decoded mesh bounding box to input mesh bounding boxes.
  BoundingBox decoded_position_bbox = decoded_mesh.ComputeBoundingBox();
  BoundingBoxD<float, 2> decoded_tex_coord_bbox =
      PmcUtils::ComputeBoundingBox<float, 2>(
          *decoded_mesh.GetNamedAttribute(GeometryAttribute::TEX_COORD));
  BoundingBox decoded_normal_bbox = PmcUtils::ComputeBoundingBox<float, 3>(
      *decoded_mesh.GetNamedAttribute(GeometryAttribute::NORMAL));

  constexpr float kTol = 0.001f;
  Compare(decoded_position_bbox, input_position_bbox, kTol, "Position");
  Compare(decoded_tex_coord_bbox, input_tex_coord_bbox, kTol, "TexCoord");
  Compare(decoded_normal_bbox, input_normal_bbox, kTol, "Normal");
}

TEST(PmcDracoMeshEncoderTest, ColorAttribute) {
  auto mesh = draco::ReadMeshFromTestFile("test_pos_color.ply");
  ASSERT_NE(mesh, nullptr);
  ASSERT_EQ(mesh->num_faces(), 224);
  ASSERT_EQ(mesh->num_points(), 114);
  ASSERT_EQ(mesh->num_attributes(), 2);

  ASSERT_EQ(mesh->NumNamedAttributes(GeometryAttribute::POSITION), 1);
  ASSERT_EQ(mesh->NumNamedAttributes(GeometryAttribute::COLOR), 1);

  ASSERT_EQ(mesh->GetNamedAttribute(GeometryAttribute::POSITION)->unique_id(),
            0);
  ASSERT_EQ(mesh->GetNamedAttribute(GeometryAttribute::COLOR)->unique_id(), 1);

  const auto* color_att = mesh->GetNamedAttribute(GeometryAttribute::COLOR);
  ASSERT_NE(color_att, nullptr);
  ASSERT_EQ(color_att->num_components(), 4);

  // Compute input mesh bounding box.
  BoundingBox input_position_bbox = mesh->ComputeBoundingBox();
  BoundingBoxD<int32_t, 4> input_color_bbox =
      PmcUtils::ComputeBoundingBox<int32_t, 4>(
          *mesh->GetNamedAttribute(GeometryAttribute::COLOR));

  // Set quantization options.
  DracoCompressionOptions& options = mesh->GetCompressionOptions();
  options.quantization_position.SetQuantizationBits(12);
  options.quantization_bits_color = 0;

  // Encode mesh with PmcDracoMeshEncoder.
  PmcDracoMeshEncoder encoder;
  EncoderBuffer out_buffer;
  ASSERT_TRUE(encoder.Encode(*mesh, &out_buffer).ok());
  ASSERT_GT(out_buffer.size(), 50);

  // Decode mesh with PmcDracoMeshDecoder.
  DecoderBuffer in_buffer;
  in_buffer.Init(out_buffer.data(), out_buffer.size());
  PmcDracoMeshDecoder decoder;
  Mesh decoded_mesh;
  ASSERT_TRUE(decoder.Decode(&in_buffer, &decoded_mesh).ok());

  // Check decoded mesh.
  ASSERT_EQ(decoded_mesh.num_faces(), 224);
  ASSERT_EQ(decoded_mesh.num_points(), 114);

  ASSERT_EQ(decoded_mesh.NumNamedAttributes(GeometryAttribute::POSITION), 1);
  ASSERT_EQ(decoded_mesh.NumNamedAttributes(GeometryAttribute::COLOR), 1);

  ASSERT_EQ(
      decoded_mesh.GetNamedAttribute(GeometryAttribute::POSITION)->unique_id(),
      0);
  ASSERT_EQ(
      decoded_mesh.GetNamedAttribute(GeometryAttribute::COLOR)->unique_id(), 1);

  // Compare decoded mesh bounding box to input mesh bounding boxes.
  BoundingBox decoded_position_bbox = decoded_mesh.ComputeBoundingBox();
  BoundingBoxD<int32_t, 4> decoded_color_bbox =
      PmcUtils::ComputeBoundingBox<int32_t, 4>(
          *decoded_mesh.GetNamedAttribute(GeometryAttribute::COLOR));

  constexpr float kTol = 0.001f;
  Compare(decoded_position_bbox, input_position_bbox, kTol, "Position");
  Compare(decoded_color_bbox, input_color_bbox, kTol, "Color");
}

TEST(PmcDracoMeshEncoderTest, TwoTexcoordAttributes) {
  auto mesh = draco::ReadMeshFromTestFile("sphere_two_tex_coords.gltf");
  ASSERT_NE(mesh, nullptr);
  ASSERT_EQ(mesh->num_faces(), 224);
  ASSERT_EQ(mesh->num_points(), 231);
  ASSERT_EQ(mesh->num_attributes(), 5);

  ASSERT_EQ(mesh->NumNamedAttributes(GeometryAttribute::POSITION), 1);
  ASSERT_EQ(mesh->NumNamedAttributes(GeometryAttribute::TEX_COORD), 2);
  ASSERT_EQ(mesh->NumNamedAttributes(GeometryAttribute::NORMAL), 1);
  ASSERT_EQ(mesh->NumNamedAttributes(GeometryAttribute::TANGENT), 1);

  ASSERT_EQ(mesh->GetNamedAttribute(GeometryAttribute::POSITION)->unique_id(),
            1);
  ASSERT_EQ(mesh->GetNamedAttribute(GeometryAttribute::NORMAL)->unique_id(), 0);
  ASSERT_EQ(mesh->GetNamedAttribute(GeometryAttribute::TANGENT)->unique_id(),
            2);
  ASSERT_EQ(
      mesh->GetNamedAttribute(GeometryAttribute::TEX_COORD, 0)->unique_id(), 3);
  ASSERT_EQ(
      mesh->GetNamedAttribute(GeometryAttribute::TEX_COORD, 1)->unique_id(), 4);

  // Compute input mesh bounding box.
  BoundingBox input_position_bbox = mesh->ComputeBoundingBox();
  BoundingBoxD<float, 4> input_tangent_bbox =
      PmcUtils::ComputeBoundingBox<float, 4>(
          *mesh->GetNamedAttribute(GeometryAttribute::TANGENT));

  // Set quantization options.
  DracoCompressionOptions& options = mesh->GetCompressionOptions();
  options.quantization_position.SetQuantizationBits(12);
  options.quantization_bits_color = 0;

  // Encode mesh with PmcDracoMeshEncoder.
  PmcDracoMeshEncoder encoder;
  EncoderBuffer out_buffer;
  ASSERT_TRUE(encoder.Encode(*mesh, &out_buffer).ok());
  ASSERT_GT(out_buffer.size(), 50);

  // Decode mesh with PmcDracoMeshDecoder using explicit attribute names.
  DecoderBuffer in_buffer;
  in_buffer.Init(out_buffer.data(), out_buffer.size());
  PmcDracoMeshDecoder decoder;
  Mesh decoded_mesh;
  std::vector<std::string> attribute_names = {"POSITION", "NORMAL", "TANGENT",
                                              "TEXCOORD_0", "TEXCOORD_1"};
  ASSERT_TRUE(decoder.Decode(&in_buffer, &decoded_mesh, attribute_names).ok());

  // Check decoded mesh.
  ASSERT_EQ(decoded_mesh.num_faces(), 224);
  ASSERT_EQ(decoded_mesh.num_points(), 231);
  ASSERT_EQ(decoded_mesh.num_attributes(), 5);

  ASSERT_EQ(decoded_mesh.NumNamedAttributes(GeometryAttribute::TEX_COORD), 2);
  ASSERT_EQ(decoded_mesh.NumNamedAttributes(GeometryAttribute::POSITION), 1);
  ASSERT_EQ(decoded_mesh.NumNamedAttributes(GeometryAttribute::NORMAL), 1);
  ASSERT_EQ(decoded_mesh.NumNamedAttributes(GeometryAttribute::TANGENT), 1);

  ASSERT_EQ(
      decoded_mesh.GetNamedAttribute(GeometryAttribute::POSITION)->unique_id(),
      1);
  ASSERT_EQ(
      decoded_mesh.GetNamedAttribute(GeometryAttribute::NORMAL)->unique_id(),
      0);
  ASSERT_EQ(
      decoded_mesh.GetNamedAttribute(GeometryAttribute::TANGENT)->unique_id(),
      2);
  ASSERT_EQ(decoded_mesh.GetNamedAttribute(GeometryAttribute::TEX_COORD, 0)
                ->unique_id(),
            3);
  ASSERT_EQ(decoded_mesh.GetNamedAttribute(GeometryAttribute::TEX_COORD, 1)
                ->unique_id(),
            4);

  // Compare decoded mesh bounding box to input mesh bounding boxes.
  BoundingBox decoded_position_bbox = decoded_mesh.ComputeBoundingBox();
  BoundingBoxD<float, 4> decoded_tangent_bbox =
      PmcUtils::ComputeBoundingBox<float, 4>(
          *mesh->GetNamedAttribute(GeometryAttribute::TANGENT));

  constexpr float kTol = 0.001f;
  Compare(decoded_position_bbox, input_position_bbox, kTol, "Position");
  Compare(decoded_tangent_bbox, input_tangent_bbox, kTol, "Tangent");
}

TEST(PmcDracoMeshEncoderTest, DecodeMissingAttributeError) {
  auto mesh = draco::ReadMeshFromTestFile("sphere_two_tex_coords.gltf");
  ASSERT_NE(mesh, nullptr);
  ASSERT_EQ(mesh->NumNamedAttributes(GeometryAttribute::POSITION), 1);
  ASSERT_EQ(mesh->NumNamedAttributes(GeometryAttribute::TEX_COORD), 2);
  ASSERT_EQ(mesh->NumNamedAttributes(GeometryAttribute::NORMAL), 1);
  ASSERT_EQ(mesh->NumNamedAttributes(GeometryAttribute::TANGENT), 1);

  // Encode mesh.
  PmcDracoMeshEncoder encoder;
  EncoderBuffer out_buffer;
  ASSERT_TRUE(encoder.Encode(*mesh, &out_buffer).ok());

  // Decode mesh and verify error.
  DecoderBuffer in_buffer;
  in_buffer.Init(out_buffer.data(), out_buffer.size());
  PmcDracoMeshDecoder decoder;
  Mesh decoded_mesh;
  std::vector<std::string> bad_attribute_names = {
      "POSITION", "NORMAL", "TANGENT", "TEXCOORD_0", "TEXCOORD_1", "COLOR"};
  Status status =
      decoder.Decode(&in_buffer, &decoded_mesh, bad_attribute_names);
  ASSERT_FALSE(status.ok());
  ASSERT_EQ(status.error_msg_string(), "Attribute with name COLOR not found.");
}

TEST(PmcDracoMeshEncoderTest, DuplicateAttributeNamesError) {
  auto mesh = draco::ReadMeshFromTestFile("sphere_two_tex_coords.gltf");
  ASSERT_NE(mesh, nullptr);
  ASSERT_EQ(mesh->NumNamedAttributes(GeometryAttribute::POSITION), 1);
  ASSERT_EQ(mesh->NumNamedAttributes(GeometryAttribute::TEX_COORD), 2);
  ASSERT_EQ(mesh->NumNamedAttributes(GeometryAttribute::NORMAL), 1);
  ASSERT_EQ(mesh->NumNamedAttributes(GeometryAttribute::TANGENT), 1);

  // Set the metadata name of normal attribute to be "TANGENT", which
  // conflicts with the existing tangent attribute name.
  auto normal_metadata = std::make_unique<draco::AttributeMetadata>();
  normal_metadata->AddEntryString("attribute_name", "TANGENT");
  const int32_t att_id = mesh->GetAttributeIdByUniqueId(0);
  mesh->AddAttributeMetadata(att_id, std::move(normal_metadata));

  PmcDracoMeshEncoder encoder;
  EncoderBuffer out_buffer;
  Status status = encoder.Encode(*mesh, &out_buffer);
  ASSERT_FALSE(status.ok());
  ASSERT_EQ(status.error_msg_string(),
            "Attribute names must be unique. Duplicate name found: TANGENT");
}

TEST(PmcDracoMeshEncoderTest, UnboundedTexCoordAttributes) {
  auto scene = draco::ReadSceneFromTestFile("UnusedTexCoords/TexCoord2.gltf");
  ASSERT_NE(scene, nullptr);
  Mesh& mesh = scene->GetMesh(draco::MeshIndex(0));
  ASSERT_EQ(mesh.NumNamedAttributes(GeometryAttribute::TEX_COORD), 8);

  // Compute input mesh bounding box.
  BoundingBox input_position_bbox = mesh.ComputeBoundingBox();

  // Set quantization options.
  DracoCompressionOptions& options = mesh.GetCompressionOptions();
  options.quantization_position.SetQuantizationBits(12);

  // Encode mesh with PmcDracoMeshEncoder.
  PmcDracoMeshEncoder encoder;
  EncoderBuffer out_buffer;
  ASSERT_TRUE(encoder.Encode(mesh, &out_buffer).ok());
  ASSERT_GT(out_buffer.size(), 50);

  // Decode mesh with PmcDracoMeshDecoder.
  DecoderBuffer in_buffer;
  in_buffer.Init(out_buffer.data(), out_buffer.size());
  PmcDracoMeshDecoder decoder;
  Mesh decoded_mesh;
  ASSERT_TRUE(decoder.Decode(&in_buffer, &decoded_mesh).ok());

  // Check decoded mesh.
  ASSERT_EQ(decoded_mesh.NumNamedAttributes(GeometryAttribute::TEX_COORD), 8);
  ASSERT_EQ(decoded_mesh.NumNamedAttributes(GeometryAttribute::POSITION), 1);

  // Compare decoded mesh bounding box to input mesh bounding box.
  BoundingBox decoded_position_bbox = decoded_mesh.ComputeBoundingBox();

  constexpr float kTol = 0.001f;
  Compare(decoded_position_bbox, input_position_bbox, kTol, "Position");

  // Compare bounding boxes of all texture coordinate attributes.
  for (int i = 0; i < 8; ++i) {
    BoundingBoxD<float, 2> input_tex_coord_bbox =
        PmcUtils::ComputeBoundingBox<float, 2>(
            *mesh.GetNamedAttribute(GeometryAttribute::TEX_COORD, i));
    BoundingBoxD<float, 2> decoded_tex_coord_bbox =
        PmcUtils::ComputeBoundingBox<float, 2>(
            *decoded_mesh.GetNamedAttribute(GeometryAttribute::TEX_COORD, i));
    Compare(decoded_tex_coord_bbox, input_tex_coord_bbox, kTol,
            "TexCoord_" + std::to_string(i));
  }
}

}  // namespace draco

#endif  // DRACO_PMC_SUPPORTED
