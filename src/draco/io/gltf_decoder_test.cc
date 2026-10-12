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
#include "draco/io/gltf_decoder.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "draco/animation/animation.h"
#include "draco/io/file_utils.h"
#include "draco/io/gltf_encoder.h"
#include "draco/material/material_library.h"
#include "draco/metadata/json_entity.h"
#include "draco/scene/audio.h"
#include "draco/scene/mesh_group.h"
#include "json.hpp"

#ifdef DRACO_TRANSCODER_SUPPORTED
#include "draco/core/constants.h"
#include "draco/core/draco_test_base.h"
#include "draco/core/draco_test_utils.h"
#include "draco/core/draco_types.h"
#include "draco/io/gltf_test_helper.h"
#include "draco/io/texture_io.h"
#include "draco/mesh/mesh_are_equivalent.h"
#include "draco/mesh/mesh_utils.h"
#include "draco/metadata/geometry_metadata.h"
#include "draco/metadata/metadata.h"
#include "draco/scene/scene_indices.h"
#include "draco/scene/scene_utils.h"
#include "draco/texture/texture_utils.h"
#include "gtest/gtest.h"

namespace draco {

namespace {
std::unique_ptr<Mesh> DecodeGltfFile(const std::string& file_name) {
  const std::string path = GetTestFileFullPath(file_name);
  GltfDecoder decoder;

  auto maybe_geometry = decoder.DecodeFromFile(path);
  if (!maybe_geometry.ok()) {
    return nullptr;
  }
  std::unique_ptr<Mesh> geometry = std::move(maybe_geometry).value();
  return geometry;
}

std::unique_ptr<Scene> DecodeGltfFileToScene(const std::string& file_name) {
  const std::string path = GetTestFileFullPath(file_name);
  GltfDecoder decoder;

  auto maybe_scene = decoder.DecodeFromFileToScene(path);
  if (!maybe_scene.ok()) {
    std::cout << maybe_scene.status().error_msg_string() << "\n";
    return nullptr;
  }
  std::unique_ptr<Scene> scene = std::move(maybe_scene).value();
  return scene;
}

void CompareVectorArray(const std::array<Vector3f, 3>& a,
                        const std::array<Vector3f, 3>& b) {
  for (int v = 0; v < 3; ++v) {
    for (int c = 0; c < 3; ++c) {
      EXPECT_FLOAT_EQ(a[v][c], b[v][c]) << "v:" << v << " c:" << c;
    }
  }
}
}  // namespace

// Tests multiple textures.
TEST(GltfDecoderTest, SphereGltf) {
  const std::string file_name = "sphere.gltf";
  const std::unique_ptr<Mesh> mesh(DecodeGltfFile(file_name));
  EXPECT_EQ(mesh->num_attributes(), 4) << "Unexpected number of attributes.";
  EXPECT_EQ(mesh->num_points(), 231) << "Unexpected number of vertices.";
  EXPECT_EQ(mesh->num_faces(), 224) << "Unexpected number of faces.";
  ASSERT_EQ(mesh->GetMaterialLibrary().NumMaterials(), 1);
  ASSERT_EQ(mesh->GetMaterialLibrary().GetMaterial(0)->NumTextureMaps(), 2);
}

TEST(GltfDecoderTest, AnimationPointerGltf) {
  const std::string file_name = "sphere_animation_pointer.gltf";
  const std::unique_ptr<Scene> scene(DecodeGltfFileToScene(file_name));
  ASSERT_NE(scene, nullptr);

  EXPECT_EQ(scene->NumAnimations(), 1);
  const Animation* animation = scene->GetAnimation(AnimationIndex(0));
  ASSERT_NE(animation, nullptr);

  ASSERT_EQ(animation->NumChannels(), 4);

  // Verify channels.
  const AnimationChannel* channel0 = animation->GetChannel(0);
  EXPECT_EQ(channel0->transformation_type,
            AnimationChannel::ChannelTransformation::POINTER);
  EXPECT_EQ(channel0->pointer,
            "/materials/0/pbrMetallicRoughness/baseColorFactor");

  const AnimationChannel* channel1 = animation->GetChannel(1);
  EXPECT_EQ(channel1->transformation_type,
            AnimationChannel::ChannelTransformation::POINTER);
  EXPECT_EQ(channel1->pointer,
            "/materials/0/pbrMetallicRoughness/baseColorTexture/extensions/"
            "KHR_texture_transform/offset");

  const AnimationChannel* channel2 = animation->GetChannel(2);
  EXPECT_EQ(channel2->transformation_type,
            AnimationChannel::ChannelTransformation::POINTER);
  EXPECT_EQ(channel2->pointer,
            "/materials/0/extensions/KHR_materials_emissive_strength/"
            "emissiveStrength");

  const AnimationChannel* channel3 = animation->GetChannel(3);
  EXPECT_EQ(channel3->transformation_type,
            AnimationChannel::ChannelTransformation::POINTER);
  EXPECT_EQ(channel3->pointer, "/nodes/0/translation");
}

TEST(GltfDecoderTest, AnimationPointerAllExtensionsGltf) {
  const std::string file_name = "sphere_animation_pointer.gltf";
  const std::string path = GetTestFileFullPath(file_name);
  std::string json_str;
  ASSERT_TRUE(ReadFileToString(path, &json_str));

  nlohmann::json data = nlohmann::json::parse(json_str);

  // Add new extensions to material 0 with non-default values to ensure they
  // are written by the encoder.
  auto& mat = data["materials"][0];
  mat["extensions"]["KHR_materials_iridescence"] = {
      {"iridescenceFactor", 1.0},
      {"iridescenceIor", 1.5},
      {"iridescenceThicknessMinimum", 200.0},
      {"iridescenceThicknessMaximum", 500.0}};
  mat["extensions"]["KHR_materials_anisotropy"] = {{"anisotropyStrength", 0.5},
                                                   {"anisotropyRotation", 1.0}};
  mat["extensions"]["KHR_materials_diffuse_transmission"] = {
      {"diffuseTransmissionFactor", 0.8},
      {"diffuseTransmissionColorFactor", {1.0, 0.5, 0.2}}};

  // Add animation channels
  auto& anim = data["animations"][0];
  anim["channels"].push_back(
      {{"sampler", 0},
       {"target",
        {{"path", "pointer"},
         {"extensions",
          {{"KHR_animation_pointer",
            {{"pointer",
              "/materials/0/extensions/KHR_materials_iridescence/"
              "iridescenceFactor"}}}}}}}});

  std::string modified_json_str = data.dump();

  // Write to temp file
  const std::string temp_path =
      GetTestTempFileFullPath("sphere_animation_pointer_all.gltf");
  ASSERT_TRUE(WriteBufferToFile(modified_json_str.data(),
                                modified_json_str.size(), temp_path));

  // Copy buffer 0
  std::vector<char> buffer0_data;
  ASSERT_TRUE(ReadFileToBuffer(GetTestFileFullPath("sphere_buffer0.bin"),
                               &buffer0_data));
  ASSERT_TRUE(WriteBufferToFile(buffer0_data.data(), buffer0_data.size(),
                                GetTestTempFileFullPath("sphere_buffer0.bin")));

  // Copy animation buffer
  std::vector<char> anim_buffer_data;
  ASSERT_TRUE(ReadFileToBuffer(GetTestFileFullPath("sphere_animation.bin"),
                               &anim_buffer_data));
  ASSERT_TRUE(
      WriteBufferToFile(anim_buffer_data.data(), anim_buffer_data.size(),
                        GetTestTempFileFullPath("sphere_animation.bin")));

  // Copy images
  std::vector<char> img0_data;
  ASSERT_TRUE(ReadFileToBuffer(
      GetTestFileFullPath("sphere_Texture0_Normal.png"), &img0_data));
  ASSERT_TRUE(
      WriteBufferToFile(img0_data.data(), img0_data.size(),
                        GetTestTempFileFullPath("sphere_Texture0_Normal.png")));

  std::vector<char> img1_data;
  ASSERT_TRUE(ReadFileToBuffer(
      GetTestFileFullPath("sphere_Texture1_BaseColor.png"), &img1_data));
  ASSERT_TRUE(WriteBufferToFile(
      img1_data.data(), img1_data.size(),
      GetTestTempFileFullPath("sphere_Texture1_BaseColor.png")));

  // Decode from file
  GltfDecoder decoder;
  auto maybe_scene = decoder.DecodeFromFileToScene(temp_path);
  if (!maybe_scene.ok()) {
    FAIL() << "Failed to decode scene: "
           << maybe_scene.status().error_msg_string();
  }
  std::unique_ptr<Scene> scene = std::move(maybe_scene).value();

  ASSERT_NE(scene, nullptr);

  // Verify material extensions
  const Material* m = scene->GetMaterialLibrary().GetMaterial(0);
  EXPECT_TRUE(m->HasIridescence());
  EXPECT_EQ(m->GetIridescenceFactor(), 1.0f);
  EXPECT_EQ(m->GetIridescenceIor(), 1.5f);
  EXPECT_EQ(m->GetIridescenceThicknessMinimum(), 200.0f);
  EXPECT_EQ(m->GetIridescenceThicknessMaximum(), 500.0f);

  EXPECT_TRUE(m->HasAnisotropy());
  EXPECT_EQ(m->GetAnisotropyStrength(), 0.5f);
  EXPECT_EQ(m->GetAnisotropyRotation(), 1.0f);

  EXPECT_TRUE(m->HasDiffuseTransmission());
  EXPECT_EQ(m->GetDiffuseTransmissionFactor(), 0.8f);
  EXPECT_EQ(m->GetDiffuseTransmissionColorFactor(), Vector3f(1.0f, 0.5f, 0.2f));

  // Verify animation channels
  const Animation* animation = scene->GetAnimation(AnimationIndex(0));
  ASSERT_NE(animation, nullptr);
  ASSERT_EQ(animation->NumChannels(), 5);
  const AnimationChannel* channel4 = animation->GetChannel(4);
  EXPECT_EQ(channel4->transformation_type,
            AnimationChannel::ChannelTransformation::POINTER);
  EXPECT_EQ(
      channel4->pointer,
      "/materials/0/extensions/KHR_materials_iridescence/iridescenceFactor");

  // Now test encoding
  GltfEncoder encoder;
  const std::string output_path =
      GetTestTempFileFullPath("sphere_animation_pointer_all_out.gltf");
  ASSERT_TRUE(encoder.EncodeFile(*scene, output_path).ok());

  // Read encoded file and verify JSON
  std::string encoded_json_str;
  ASSERT_TRUE(ReadFileToString(output_path, &encoded_json_str));
  nlohmann::json encoded_data = nlohmann::json::parse(encoded_json_str);

  // Verify extensions are present in encoded JSON
  auto& encoded_mat = encoded_data["materials"][0];
  EXPECT_TRUE(encoded_mat["extensions"].contains("KHR_materials_iridescence"));
  EXPECT_TRUE(encoded_mat["extensions"].contains("KHR_materials_anisotropy"));
  EXPECT_TRUE(
      encoded_mat["extensions"].contains("KHR_materials_diffuse_transmission"));

  // Verify non-default values are preserved
  auto& encoded_irid = encoded_mat["extensions"]["KHR_materials_iridescence"];
  EXPECT_EQ(encoded_irid["iridescenceFactor"], 1.0);
  EXPECT_EQ(encoded_irid["iridescenceIor"], 1.5);
  EXPECT_EQ(encoded_irid["iridescenceThicknessMinimum"], 200.0);
  EXPECT_EQ(encoded_irid["iridescenceThicknessMaximum"], 500.0);
}

TEST(GltfDecoderTest, TriangleGltf) {
  const std::string file_name = "one_face_123.gltf";
  const std::unique_ptr<Mesh> mesh(DecodeGltfFile(file_name));
  EXPECT_EQ(mesh->num_attributes(), 1) << "Unexpected number of attributes.";
  EXPECT_EQ(mesh->num_points(), 3) << "Unexpected number of vertices.";
  EXPECT_EQ(mesh->num_faces(), 1) << "Unexpected number of faces.";
  ASSERT_EQ(mesh->GetMaterialLibrary().NumMaterials(), 1);
  ASSERT_EQ(mesh->GetMaterialLibrary().GetMaterial(0)->NumTextureMaps(), 0);

  const auto* const pos_attribute =
      mesh->GetNamedAttribute(GeometryAttribute::POSITION);
  EXPECT_NE(pos_attribute, nullptr);
  const auto& face = mesh->face(FaceIndex(0));
  std::array<Vector3f, 3> pos;
  for (int c = 0; c < 3; ++c) {
    pos_attribute->GetMappedValue(face[c], &pos[c][0]);
  }

  // Test position values match.
  std::array<Vector3f, 3> pos_test;
  pos_test[0] = Vector3f(1, 0.0999713, 0);
  pos_test[1] = Vector3f(2.00006104, 0.01, 0);
  pos_test[2] = Vector3f(3, 0.10998169, 0);
  CompareVectorArray(pos, pos_test);
}

TEST(GltfDecoderTest, TriangleGltfCesiumRtc) {
  const std::string file_name = "one_face_123_cesium_rtc.gltf";
  const std::unique_ptr<Mesh> mesh(DecodeGltfFile(file_name));
  EXPECT_EQ(mesh->num_attributes(), 1) << "Unexpected number of attributes.";
  EXPECT_EQ(mesh->num_points(), 3) << "Unexpected number of vertices.";
  EXPECT_EQ(mesh->num_faces(), 1) << "Unexpected number of faces.";
  ASSERT_EQ(mesh->GetMaterialLibrary().NumMaterials(), 1);
  ASSERT_EQ(mesh->GetMaterialLibrary().GetMaterial(0)->NumTextureMaps(), 0);

  const auto* const pos_attribute =
      mesh->GetNamedAttribute(GeometryAttribute::POSITION);
  EXPECT_NE(pos_attribute, nullptr);
  const auto& face = mesh->face(FaceIndex(0));
  std::array<Vector3f, 3> pos;
  for (int c = 0; c < 3; ++c) {
    pos_attribute->GetMappedValue(face[c], &pos[c][0]);
  }

  // Test position values match.
  std::array<Vector3f, 3> pos_test;
  pos_test[0] = Vector3f(1, 0.0999713, 0);
  pos_test[1] = Vector3f(2.00006104, 0.01, 0);
  pos_test[2] = Vector3f(3, 0.10998169, 0);
  CompareVectorArray(pos, pos_test);

  const std::unique_ptr<Scene> scene(DecodeGltfFileToScene(file_name));
  const std::vector<double> expected_cesium_rtc = {-123.4, 234.5, 345678.9};
  EXPECT_EQ(scene->GetCesiumRtc(), expected_cesium_rtc);
}

TEST(GltfDecoderTest, MirroredTriangleGltf) {
  const std::string file_name = "one_face_123_mirror.gltf";
  const std::unique_ptr<Mesh> mesh(DecodeGltfFile(file_name));
  EXPECT_EQ(mesh->num_attributes(), 1) << "Unexpected number of attributes.";
  EXPECT_EQ(mesh->num_points(), 3) << "Unexpected number of vertices.";
  EXPECT_EQ(mesh->num_faces(), 1) << "Unexpected number of faces.";
  ASSERT_EQ(mesh->GetMaterialLibrary().NumMaterials(), 1);
  ASSERT_EQ(mesh->GetMaterialLibrary().GetMaterial(0)->NumTextureMaps(), 0);

  const auto* const pos_attribute =
      mesh->GetNamedAttribute(GeometryAttribute::POSITION);
  EXPECT_NE(pos_attribute, nullptr);
  const auto& face = mesh->face(FaceIndex(0));
  std::array<Vector3f, 3> pos;
  for (int c = 0; c < 3; ++c) {
    pos_attribute->GetMappedValue(face[c], &pos[c][0]);
  }

  // Test position values match.
  std::array<Vector3f, 3> pos_test;
  pos_test[0] = Vector3f(-1, -0.0999713, 0);
  pos_test[1] = Vector3f(-3, -0.10998169, 0);
  pos_test[2] = Vector3f(-2.00006104, -0.01, 0);
  CompareVectorArray(pos, pos_test);
}

TEST(GltfDecoderTest, TranslateTriangleGltf) {
  const std::string file_name = "one_face_123_translated.gltf";
  const std::unique_ptr<Mesh> mesh(DecodeGltfFile(file_name));
  EXPECT_EQ(mesh->num_attributes(), 1) << "Unexpected number of attributes.";
  EXPECT_EQ(mesh->num_points(), 3) << "Unexpected number of vertices.";
  EXPECT_EQ(mesh->num_faces(), 1) << "Unexpected number of faces.";
  ASSERT_EQ(mesh->GetMaterialLibrary().NumMaterials(), 1);
  ASSERT_EQ(mesh->GetMaterialLibrary().GetMaterial(0)->NumTextureMaps(), 0);

  const auto* const pos_attribute =
      mesh->GetNamedAttribute(GeometryAttribute::POSITION);
  EXPECT_NE(pos_attribute, nullptr);
  const auto& face = mesh->face(FaceIndex(0));
  std::array<Vector3f, 3> pos;
  for (int c = 0; c < 3; ++c) {
    pos_attribute->GetMappedValue(face[c], &pos[c][0]);
  }

  // Test position values match. The glTF file contains a matrix in the main
  // node. The matrix defines a translation of (-1.5, 5.0, 2.3).
  std::array<Vector3f, 3> pos_test;
  pos_test[0] = Vector3f(1, 0.0999713, 0);
  pos_test[1] = Vector3f(2.00006104, 0.01, 0);
  pos_test[2] = Vector3f(3, 0.10998169, 0);
  const Vector3f translate(-1.5, 5.0, 2.3);
  for (int v = 0; v < 3; ++v) {
    pos_test[v] = pos_test[v] + translate;
  }
  CompareVectorArray(pos, pos_test);
}

// Tests multiple materials.
TEST(GltfDecoderTest, MilkTruckGltf) {
  const std::string file_name = "CesiumMilkTruck/glTF/CesiumMilkTruck.gltf";
  const std::unique_ptr<Mesh> mesh(DecodeGltfFile(file_name));
  EXPECT_EQ(mesh->num_attributes(), 4) << "Unexpected number of attributes.";
  EXPECT_EQ(mesh->num_points(), 3564) << "Unexpected number of vertices.";
  EXPECT_EQ(mesh->num_faces(), 3624) << "Unexpected number of faces.";
  ASSERT_EQ(mesh->GetMaterialLibrary().NumMaterials(), 4);
  ASSERT_EQ(mesh->GetMaterialLibrary().GetMaterial(0)->NumTextureMaps(), 1);
  ASSERT_EQ(mesh->GetMaterialLibrary().GetMaterial(1)->NumTextureMaps(), 0);
  ASSERT_EQ(mesh->GetMaterialLibrary().GetMaterial(2)->NumTextureMaps(), 0);
  ASSERT_EQ(mesh->GetMaterialLibrary().GetMaterial(3)->NumTextureMaps(), 1);
  ASSERT_EQ(mesh->GetMaterialLibrary().GetMaterial(0)->GetName(), "truck");
  ASSERT_EQ(mesh->GetMaterialLibrary().GetMaterial(1)->GetName(), "glass");
  ASSERT_EQ(mesh->GetMaterialLibrary().GetMaterial(2)->GetName(),
            "window_trim");
  ASSERT_EQ(mesh->GetMaterialLibrary().GetMaterial(3)->GetName(), "wheels");
}

TEST(GltfDecoderTest, SceneMilkTruckGltf) {
  const std::string file_name = "CesiumMilkTruck/glTF/CesiumMilkTruck.gltf";
  const std::unique_ptr<Scene> scene(DecodeGltfFileToScene(file_name));

  ASSERT_EQ(scene->NumMeshes(), 4);
  ASSERT_EQ(scene->NumMeshGroups(), 2);
  ASSERT_EQ(scene->NumNodes(), 5);
  ASSERT_EQ(scene->NumRootNodes(), 1);
  ASSERT_EQ(scene->NumLights(), 0);
  ASSERT_EQ(scene->GetMaterialLibrary().NumMaterials(), 4);
  ASSERT_EQ(scene->GetMaterialLibrary().GetMaterial(0)->NumTextureMaps(), 1);
  ASSERT_EQ(scene->GetMaterialLibrary().GetMaterial(1)->NumTextureMaps(), 0);
  ASSERT_EQ(scene->GetMaterialLibrary().GetMaterial(2)->NumTextureMaps(), 0);
  ASSERT_EQ(scene->GetMaterialLibrary().GetMaterial(3)->NumTextureMaps(), 1);
  ASSERT_EQ(scene->GetMaterialLibrary().GetMaterial(0)->GetName(), "truck");
  ASSERT_EQ(scene->GetMaterialLibrary().GetMaterial(1)->GetName(), "glass");
  ASSERT_EQ(scene->GetMaterialLibrary().GetMaterial(2)->GetName(),
            "window_trim");
  ASSERT_EQ(scene->GetMaterialLibrary().GetMaterial(3)->GetName(), "wheels");
  ASSERT_EQ(scene->NumAnimations(), 1);
  ASSERT_EQ(scene->NumSkins(), 0);
  for (AnimationIndex i(0); i < scene->NumAnimations(); ++i) {
    const Animation* const animation = scene->GetAnimation(i);
    ASSERT_NE(animation, nullptr);
    ASSERT_EQ(animation->NumSamplers(), 2);
    ASSERT_EQ(animation->NumChannels(), 2);
  }

  ASSERT_EQ(scene->GetMeshGroup(MeshGroupIndex(0))->GetName(),
            "Cesium_Milk_Truck");
  ASSERT_EQ(scene->GetMeshGroup(MeshGroupIndex(1))->GetName(), "Wheels");

  // Check all of the meshes do not have any materials.
  for (MeshIndex i(0); i < scene->NumMeshes(); ++i) {
    const Mesh& mesh = scene->GetMesh(i);
    ASSERT_EQ(mesh.GetMaterialLibrary().NumMaterials(), 0);
  }
}

TEST(GltfDecoderTest, AnimatedBonesGltf) {
  const std::string file_name = "CesiumMan/glTF/CesiumMan.gltf";
  const std::unique_ptr<Scene> scene(DecodeGltfFileToScene(file_name));

  ASSERT_EQ(scene->NumMeshes(), 1);
  ASSERT_EQ(scene->NumMeshGroups(), 1);
  const MeshGroup& mesh_group = *scene->GetMeshGroup(MeshGroupIndex(0));
  ASSERT_EQ(mesh_group.NumMeshInstances(), 1);
  ASSERT_EQ(mesh_group.GetMeshInstance(0).material_index, 0);
  ASSERT_EQ(scene->NumNodes(), 22);
  ASSERT_EQ(scene->NumRootNodes(), 1);
  ASSERT_EQ(scene->GetMaterialLibrary().NumMaterials(), 1);
  ASSERT_EQ(scene->GetMaterialLibrary().GetMaterial(0)->NumTextureMaps(), 1);
  ASSERT_EQ(scene->NumAnimations(), 1);
  ASSERT_EQ(scene->NumSkins(), 1);
  for (AnimationIndex i(0); i < scene->NumAnimations(); ++i) {
    const Animation* const animation = scene->GetAnimation(i);
    ASSERT_NE(animation, nullptr);
    ASSERT_EQ(animation->NumSamplers(), 57);
    ASSERT_EQ(animation->NumChannels(), 57);
  }

  // Check all of the meshes do not have any materials.
  for (MeshIndex i(0); i < scene->NumMeshes(); ++i) {
    const Mesh& mesh = scene->GetMesh(i);
    ASSERT_EQ(mesh.GetMaterialLibrary().NumMaterials(), 0);
  }
}

TEST(GltfDecoderTest, AnimatedBonesGlb) {
  const std::string file_name = "CesiumMan/glTF_Binary/CesiumMan.glb";
  const std::unique_ptr<Scene> scene(DecodeGltfFileToScene(file_name));
  ASSERT_EQ(scene->NumMeshes(), 1);
  ASSERT_EQ(scene->NumMeshGroups(), 1);
  const MeshGroup& mesh_group = *scene->GetMeshGroup(MeshGroupIndex(0));
  ASSERT_EQ(mesh_group.NumMeshInstances(), 1);
  ASSERT_EQ(mesh_group.GetMeshInstance(0).material_index, 0);
  ASSERT_EQ(scene->NumNodes(), 22);
  ASSERT_EQ(scene->NumRootNodes(), 1);
  ASSERT_EQ(scene->GetMaterialLibrary().NumMaterials(), 1);
  ASSERT_EQ(scene->GetMaterialLibrary().GetMaterial(0)->NumTextureMaps(), 1);
  ASSERT_EQ(scene->NumAnimations(), 1);
  ASSERT_EQ(scene->NumSkins(), 1);
  for (AnimationIndex i(0); i < scene->NumAnimations(); ++i) {
    const Animation* const animation = scene->GetAnimation(i);
    ASSERT_NE(animation, nullptr);
    ASSERT_EQ(animation->NumSamplers(), 57);
    ASSERT_EQ(animation->NumChannels(), 57);
  }

  // Check all of the meshes do not have any materials.
  for (MeshIndex i(0); i < scene->NumMeshes(); ++i) {
    const Mesh& mesh = scene->GetMesh(i);
    ASSERT_EQ(mesh.GetMaterialLibrary().NumMaterials(), 0);
  }
}

// Tests multiple primitives with the same material index.
TEST(GltfDecoderTest, LanternGltf) {
  const std::string file_name = "Lantern/glTF/Lantern.gltf";
  const std::unique_ptr<Mesh> mesh(DecodeGltfFile(file_name));

  EXPECT_EQ(mesh->num_attributes(), 4) << "Unexpected number of attributes.";
  EXPECT_EQ(mesh->num_points(), 4145) << "Unexpected number of vertices.";
  EXPECT_EQ(mesh->num_faces(), 5394) << "Unexpected number of faces.";
  ASSERT_EQ(mesh->GetMaterialLibrary().NumMaterials(), 1);
  ASSERT_EQ(mesh->GetMaterialLibrary().GetMaterial(0)->NumTextureMaps(), 4);
}

// Tests COLOR_0 input attribute.
TEST(GltfDecoderTest, ColorAttributeGltf) {
  const std::string file_name = "test_pos_color.gltf";
  const std::unique_ptr<Mesh> mesh(DecodeGltfFile(file_name));
  EXPECT_EQ(mesh->num_attributes(), 2) << "Unexpected number of attributes.";
  EXPECT_EQ(mesh->num_points(), 114) << "Unexpected number of vertices.";
  EXPECT_EQ(mesh->num_faces(), 224) << "Unexpected number of faces.";
  ASSERT_EQ(mesh->GetMaterialLibrary().NumMaterials(), 1);
  ASSERT_EQ(mesh->GetMaterialLibrary().GetMaterial(0)->NumTextureMaps(), 0);
  ASSERT_NE(mesh->GetNamedAttribute(GeometryAttribute::COLOR), nullptr);
  ASSERT_EQ(mesh->GetNamedAttribute(GeometryAttribute::COLOR)->data_type(),
            draco::DT_UINT8);
  // Ensure the normalized property for the color attribute is set properly.
  ASSERT_TRUE(mesh->GetNamedAttribute(GeometryAttribute::COLOR)->normalized());
}

// Tests COLOR_0 input attribute when the asset is loaded into a scene.
TEST(GltfDecoderTest, ColorAttributeGltfScene) {
  const std::string file_name = "test_pos_color.gltf";
  const std::unique_ptr<Scene> scene(DecodeGltfFileToScene(file_name));
  ASSERT_EQ(scene->NumMeshes(), 1);
  const Mesh& mesh = scene->GetMesh(MeshIndex(0));
  ASSERT_NE(mesh.GetNamedAttribute(GeometryAttribute::COLOR), nullptr);
  ASSERT_EQ(mesh.GetNamedAttribute(GeometryAttribute::COLOR)->data_type(),
            draco::DT_UINT8);
  // Ensure the normalized property for the color attribute is set properly.
  ASSERT_TRUE(mesh.GetNamedAttribute(GeometryAttribute::COLOR)->normalized());
}

// Tests a mesh with two sets of texture coordinates.
TEST(GltfDecoderTest, TwoTexCoordAttributesGltf) {
  const std::string file_name = "sphere_two_tex_coords.gltf";
  const std::unique_ptr<Mesh> mesh(DecodeGltfFile(file_name));
  ASSERT_NE(mesh, nullptr);
  ASSERT_EQ(mesh->NumNamedAttributes(GeometryAttribute::TEX_COORD), 2);
}

// Tests an input with a valid tangent attribute does not auto generate the
// tangent attribute.
TEST(GltfDecoderTest, TestSceneWithTangents) {
  const std::string file_name = "Lantern/glTF/Lantern.gltf";
  const std::unique_ptr<Scene> scene(DecodeGltfFileToScene(file_name));
  ASSERT_NE(scene, nullptr);

  // Ensure no mesh has auto-generated tangents (and that some meshes have the
  // tangent attribute).
  int num_tangent_attributes = 0;
  for (MeshIndex mi(0); mi < scene->NumMeshes(); ++mi) {
    if (scene->GetMesh(mi).GetNamedAttribute(GeometryAttribute::TANGENT) !=
        nullptr) {
      num_tangent_attributes++;
      ASSERT_FALSE(MeshUtils::HasAutoGeneratedTangents(scene->GetMesh(mi)));
    }
  }
  ASSERT_GT(num_tangent_attributes, 0);
}

// Tests an input file where multiple textures share the same image asset.
TEST(GltfDecoderTest, SharedImages) {
  const std::string file_name = "SphereAllSame/sphere_texture_all.gltf";
  const std::unique_ptr<Mesh> mesh(DecodeGltfFile(file_name));
  ASSERT_NE(mesh, nullptr);
  ASSERT_EQ(mesh->GetMaterialLibrary().NumMaterials(), 1);
  ASSERT_EQ(mesh->GetMaterialLibrary().GetMaterial(0)->NumTextureMaps(), 5);
  ASSERT_EQ(mesh->GetMaterialLibrary().GetTextureLibrary().NumTextures(), 4);
}

TEST(GltfDecoderTest, TextureNamesAreNotEmpty) {
  const std::string file_name = "SphereAllSame/sphere_texture_all.gltf";
  const std::unique_ptr<Mesh> mesh(DecodeGltfFile(file_name));
  ASSERT_NE(mesh, nullptr);
  ASSERT_EQ(mesh->GetMaterialLibrary().NumMaterials(), 1);
  ASSERT_EQ(mesh->GetMaterialLibrary().GetMaterial(0)->NumTextureMaps(), 5);
  ASSERT_EQ(mesh->GetMaterialLibrary().GetTextureLibrary().NumTextures(), 4);
  const std::vector<const draco::Texture*> textures = {
      mesh->GetMaterialLibrary().GetTextureLibrary().GetTexture(0),
      mesh->GetMaterialLibrary().GetTextureLibrary().GetTexture(1),
      mesh->GetMaterialLibrary().GetTextureLibrary().GetTexture(2),
      mesh->GetMaterialLibrary().GetTextureLibrary().GetTexture(3)};
  EXPECT_EQ(TextureUtils::GetTargetStem(*textures[0]), "256x256_all_orange");
  EXPECT_EQ(TextureUtils::GetTargetStem(*textures[1]), "256x256_all_blue");
  EXPECT_EQ(TextureUtils::GetTargetStem(*textures[2]), "256x256_all_red");
  EXPECT_EQ(TextureUtils::GetTargetStem(*textures[3]), "256x256_all_green");
  EXPECT_EQ(TextureUtils::GetTargetFormat(*textures[0]), ImageFormat::PNG);
  EXPECT_EQ(TextureUtils::GetTargetFormat(*textures[1]), ImageFormat::PNG);
  EXPECT_EQ(TextureUtils::GetTargetFormat(*textures[2]), ImageFormat::PNG);
  EXPECT_EQ(TextureUtils::GetTargetFormat(*textures[3]), ImageFormat::PNG);
}

TEST(GltfDecoderTest, TestTexCoord1) {
  const std::string file_name = "MultiUVTest/glTF/MultiUVTest.gltf";
  const std::unique_ptr<Mesh> mesh(DecodeGltfFile(file_name));
  ASSERT_NE(mesh, nullptr);
  ASSERT_EQ(mesh->GetMaterialLibrary().NumMaterials(), 1);
  ASSERT_EQ(mesh->GetMaterialLibrary().GetMaterial(0)->NumTextureMaps(), 2);
  ASSERT_EQ(mesh->GetMaterialLibrary().GetTextureLibrary().NumTextures(), 2);
  const std::vector<const draco::Texture*> textures = {
      mesh->GetMaterialLibrary().GetTextureLibrary().GetTexture(0),
      mesh->GetMaterialLibrary().GetTextureLibrary().GetTexture(1)};
  EXPECT_EQ(TextureUtils::GetTargetStem(*textures[0]), "uv0");
  EXPECT_EQ(TextureUtils::GetTargetStem(*textures[1]), "uv1");
  EXPECT_EQ(TextureUtils::GetTargetFormat(*textures[0]), ImageFormat::PNG);
  EXPECT_EQ(TextureUtils::GetTargetFormat(*textures[1]), ImageFormat::PNG);
  ASSERT_EQ(mesh->NumNamedAttributes(GeometryAttribute::TEX_COORD), 2);
  ASSERT_EQ(mesh->NumNamedAttributes(GeometryAttribute::POSITION), 1);
  ASSERT_EQ(mesh->NumNamedAttributes(GeometryAttribute::NORMAL), 1);
  ASSERT_EQ(mesh->NumNamedAttributes(GeometryAttribute::TANGENT), 1);
}

TEST(GltfDecoderTest, SimpleScene) {
  const std::string file_name = "Box/glTF/Box.gltf";
  const std::unique_ptr<Scene> scene(DecodeGltfFileToScene(file_name));

  ASSERT_EQ(scene->NumMeshes(), 1);
  ASSERT_EQ(scene->NumMeshGroups(), 1);
  const MeshGroup& mesh_group = *scene->GetMeshGroup(MeshGroupIndex(0));
  ASSERT_EQ(mesh_group.NumMeshInstances(), 1);
  ASSERT_EQ(mesh_group.GetMeshInstance(0).material_index, 0);
  ASSERT_EQ(scene->NumNodes(), 2);
  ASSERT_EQ(scene->NumRootNodes(), 1);
  ASSERT_EQ(scene->GetMaterialLibrary().NumMaterials(), 1);
  ASSERT_EQ(scene->GetMaterialLibrary().GetMaterial(0)->NumTextureMaps(), 0);
  ASSERT_EQ(scene->NumSkins(), 0);
  ASSERT_EQ(scene->NumAnimations(), 0);

  // Check all of the meshes do not have any materials.
  for (MeshIndex i(0); i < scene->NumMeshes(); ++i) {
    const Mesh& mesh = scene->GetMesh(i);
    ASSERT_EQ(mesh.GetMaterialLibrary().NumMaterials(), 0);
  }

  // Check names of nodes are empty.
  EXPECT_TRUE(scene->GetNode(SceneNodeIndex(0))->GetName().empty());
  EXPECT_TRUE(scene->GetNode(SceneNodeIndex(1))->GetName().empty());
}

TEST(GltfDecoderTest, LanternScene) {
  const std::string file_name = "Lantern/glTF/Lantern.gltf";
  const std::unique_ptr<Scene> scene(DecodeGltfFileToScene(file_name));

  EXPECT_EQ(scene->NumMeshes(), 3);
  EXPECT_EQ(scene->NumMeshGroups(), 3);
  EXPECT_EQ(scene->NumNodes(), 4);
  EXPECT_EQ(scene->NumRootNodes(), 1);
  EXPECT_EQ(scene->GetMaterialLibrary().NumMaterials(), 1);
  EXPECT_EQ(scene->GetMaterialLibrary().GetMaterial(0)->NumTextureMaps(), 4);
  EXPECT_EQ(scene->GetMaterialLibrary().GetMaterial(0)->GetDoubleSided(),
            false);
  EXPECT_EQ(scene->NumSkins(), 0);
  EXPECT_EQ(scene->NumAnimations(), 0);

  // Check names of nodes have been populated.
  EXPECT_EQ(scene->GetNode(SceneNodeIndex(0))->GetName(), "Lantern");
  EXPECT_EQ(scene->GetNode(SceneNodeIndex(1))->GetName(), "LanternPole_Body");
  EXPECT_EQ(scene->GetNode(SceneNodeIndex(2))->GetName(), "LanternPole_Chain");
  EXPECT_EQ(scene->GetNode(SceneNodeIndex(3))->GetName(),
            "LanternPole_Lantern");
}

TEST(GltfDecoderTest, SimpleTriangleMesh) {
  const std::string file_name = "Triangle/glTF/Triangle.gltf";
  const std::unique_ptr<Mesh> mesh(DecodeGltfFile(file_name));

  EXPECT_EQ(mesh->num_attributes(), 1) << "Unexpected number of attributes.";
  EXPECT_EQ(mesh->num_points(), 3) << "Unexpected number of vertices.";
  EXPECT_EQ(mesh->num_faces(), 1) << "Unexpected number of faces.";
  EXPECT_EQ(mesh->GetMaterialLibrary().NumMaterials(), 0);
}

TEST(GltfDecoderTest, SimpleTriangleScene) {
  const std::string file_name = "Triangle/glTF/Triangle.gltf";
  const std::unique_ptr<Scene> scene(DecodeGltfFileToScene(file_name));

  EXPECT_EQ(scene->NumMeshes(), 1);
  EXPECT_EQ(scene->NumMeshGroups(), 1);
  const MeshGroup& mesh_group = *scene->GetMeshGroup(MeshGroupIndex(0));
  ASSERT_EQ(mesh_group.NumMeshInstances(), 1);
  ASSERT_EQ(mesh_group.GetMeshInstance(0).material_index, 0);
  EXPECT_EQ(scene->NumNodes(), 1);
  EXPECT_EQ(scene->NumRootNodes(), 1);
  EXPECT_EQ(scene->GetMaterialLibrary().NumMaterials(), 1);
  EXPECT_EQ(scene->NumSkins(), 0);
  EXPECT_EQ(scene->NumAnimations(), 0);
}

TEST(GltfDecoderTest, ThreeMeshesOneNoMaterialScene) {
  const std::string file_name =
      "three_meshes_two_materials_one_no_material/"
      "three_meshes_two_materials_one_no_material.gltf";
  const std::unique_ptr<Scene> scene(DecodeGltfFileToScene(file_name));

  EXPECT_EQ(scene->NumMeshes(), 3);
  EXPECT_EQ(scene->NumMeshGroups(), 3);
  EXPECT_EQ(scene->NumNodes(), 4);
  EXPECT_EQ(scene->NumRootNodes(), 1);
  EXPECT_EQ(scene->GetMaterialLibrary().NumMaterials(), 3);
  EXPECT_EQ(scene->NumSkins(), 0);
  EXPECT_EQ(scene->NumAnimations(), 0);
}

TEST(GltfDecoderTest, ThreeMeshesOneNoMaterialMesh) {
  const std::string file_name =
      "three_meshes_two_materials_one_no_material/"
      "three_meshes_two_materials_one_no_material.gltf";
  const std::unique_ptr<Mesh> mesh(DecodeGltfFile(file_name));

  EXPECT_EQ(mesh->num_attributes(), 4) << "Unexpected number of attributes.";
  EXPECT_EQ(mesh->num_points(), 72) << "Unexpected number of vertices.";
  EXPECT_EQ(mesh->num_faces(), 36) << "Unexpected number of faces.";
  EXPECT_EQ(mesh->GetMaterialLibrary().NumMaterials(), 3);
}

TEST(GltfDecoderTest, DoubleSidedMaterial) {
  const std::string file_name = "TwoSidedPlane/glTF/TwoSidedPlane.gltf";
  const std::unique_ptr<Mesh> mesh(DecodeGltfFile(file_name));
  EXPECT_EQ(mesh->GetMaterialLibrary().NumMaterials(), 1);
  EXPECT_EQ(mesh->GetMaterialLibrary().GetMaterial(0)->GetDoubleSided(), true);

  const std::unique_ptr<Scene> scene(DecodeGltfFileToScene(file_name));
  EXPECT_EQ(scene->GetMaterialLibrary().NumMaterials(), 1);
  EXPECT_EQ(scene->GetMaterialLibrary().GetMaterial(0)->GetDoubleSided(), true);
}

TEST(GltfDecoderTest, VertexColorTest) {
  const std::string file_name = "VertexColorTest/glTF/VertexColorTest.gltf";
  const std::unique_ptr<Mesh> mesh(DecodeGltfFile(file_name));
  EXPECT_EQ(mesh->GetMaterialLibrary().NumMaterials(), 2);
  EXPECT_EQ(mesh->NumNamedAttributes(GeometryAttribute::COLOR), 1);

  const std::unique_ptr<Scene> scene(DecodeGltfFileToScene(file_name));
  EXPECT_EQ(scene->GetMaterialLibrary().NumMaterials(), 2);
  EXPECT_EQ(scene->NumMeshes(), 2);
  const Mesh& second_mesh = scene->GetMesh(MeshIndex(1));
  EXPECT_EQ(second_mesh.NumNamedAttributes(GeometryAttribute::COLOR), 1);
}

TEST(GltfDecoderTest, SparseAccessors) {
  const std::string filename =
      "KhronosSampleModels/SimpleSparseAccessor/glTF/SimpleSparseAccessor.gltf";
  const std::string path = GetTestFileFullPath(filename);
  GltfDecoder decoder;
  const auto maybe_scene = decoder.DecodeFromFileToScene(path);
  EXPECT_FALSE(maybe_scene.ok());
  EXPECT_EQ(maybe_scene.status().code(), Status::Code::UNSUPPORTED_FEATURE);
}

TEST(GltfDecoderTest, PbrSpecularGlossinessExtension) {
  const std::string filename =
      "KhronosSampleModels/SpecGlossVsMetalRough/glTF/"
      "SpecGlossVsMetalRough.gltf";
  const std::string path = GetTestFileFullPath(filename);
  GltfDecoder decoder;
  const auto maybe_scene = decoder.DecodeFromFileToScene(path);
  EXPECT_FALSE(maybe_scene.ok());
  EXPECT_EQ(maybe_scene.status().code(), Status::Code::UNSUPPORTED_FEATURE);
}

TEST(GltfDecoderTest, DifferentWrappingModes) {
  const std::string filename =
      "KhronosSampleModels/TextureSettingsTest/glTF/TextureSettingsTest.gltf";
  const std::string path = GetTestFileFullPath(filename);
  GltfDecoder decoder;
  const auto maybe_scene = decoder.DecodeFromFileToScene(path);
  EXPECT_TRUE(maybe_scene.ok());
  const draco::Scene& scene = *maybe_scene.value();
  ASSERT_EQ(scene.GetMaterialLibrary().GetTextureLibrary().NumTextures(), 3);
  ASSERT_EQ(scene.GetMaterialLibrary().NumMaterials(), 10);
  const draco::Material& material = *scene.GetMaterialLibrary().GetMaterial(0);
  ASSERT_EQ(material.NumTextureMaps(), 1);
  ASSERT_EQ(material.GetTextureMapByIndex(0)->wrapping_mode().s,
            draco::TextureMap::REPEAT);
  ASSERT_EQ(material.GetTextureMapByIndex(0)->wrapping_mode().t,
            draco::TextureMap::MIRRORED_REPEAT);
}

TEST(GltfDecoderTest, KhrMaterialsUnlitExtension) {
  const std::string no_unlit_filename = "Box/glTF/Box.gltf";
  const std::unique_ptr<Scene> scene_no_unlit(
      DecodeGltfFileToScene(no_unlit_filename));
  EXPECT_EQ(scene_no_unlit->GetMaterialLibrary().NumMaterials(), 1);
  EXPECT_EQ(scene_no_unlit->GetMaterialLibrary().GetMaterial(0)->GetUnlit(),
            false);

  const std::string filename =
      "KhronosSampleModels/UnlitTest/glTF/UnlitTest.gltf";
  const std::unique_ptr<Mesh> mesh(DecodeGltfFile(filename));
  EXPECT_EQ(mesh->GetMaterialLibrary().NumMaterials(), 2);
  EXPECT_EQ(mesh->GetMaterialLibrary().GetMaterial(0)->GetUnlit(), true);
  EXPECT_EQ(mesh->GetMaterialLibrary().GetMaterial(1)->GetUnlit(), true);

  const std::unique_ptr<Scene> scene(DecodeGltfFileToScene(filename));
  EXPECT_EQ(scene->GetMaterialLibrary().NumMaterials(), 2);
  EXPECT_EQ(scene->GetMaterialLibrary().GetMaterial(0)->GetUnlit(), true);
  EXPECT_EQ(scene->GetMaterialLibrary().GetMaterial(1)->GetUnlit(), true);
}

TEST(GltfDecoderTest, KhrMaterialsSheenExtension) {
  // Check that a model with no sheen is loaded with no sheen.
  {
    const std::unique_ptr<Scene> scene(
        DecodeGltfFileToScene("Box/glTF/Box.gltf"));
    EXPECT_EQ(scene->GetMaterialLibrary().NumMaterials(), 1);

    // Check that material has no sheen.
    const Material& material = *scene->GetMaterialLibrary().GetMaterial(0);
    EXPECT_FALSE(material.HasSheen());

    // Check that sheen color and roughness factors have default values.
    EXPECT_EQ(material.GetSheenColorFactor(), Vector3f(0.f, 0.f, 0.f));
    EXPECT_EQ(material.GetSheenRoughnessFactor(), 0.f);

    // Check that sheen textures are absent.
    EXPECT_EQ(material.GetTextureMapByType(TextureMap::SHEEN_COLOR), nullptr);
    EXPECT_EQ(material.GetTextureMapByType(TextureMap::SHEEN_ROUGHNESS),
              nullptr);
  }

  // Check that a model with sheen is loaded as a mesh with sheen.
  {
    // Load model as a mesh.
    const std::unique_ptr<Mesh> mesh(
        DecodeGltfFile("KhronosSampleModels/SheenCloth/glTF/SheenCloth.gltf"));
    EXPECT_NE(mesh, nullptr);
    const Material& material = *mesh->GetMaterialLibrary().GetMaterial(0);

    // Check that material has sheen.
    EXPECT_TRUE(material.HasSheen());

    // Check that sheen color and roughness factors are present.
    EXPECT_EQ(material.GetSheenColorFactor(), Vector3f(1.f, 1.f, 1.f));
    EXPECT_EQ(material.GetSheenRoughnessFactor(), 1.f);

    // Check that sheen color and roughness textures are present.
    EXPECT_NE(material.GetTextureMapByType(TextureMap::SHEEN_COLOR), nullptr);
    EXPECT_NE(material.GetTextureMapByType(TextureMap::SHEEN_ROUGHNESS),
              nullptr);

    // Check that sheen color and roughness textures are shared.
    EXPECT_EQ(
        material.GetTextureMapByType(TextureMap::SHEEN_COLOR)->texture(),
        material.GetTextureMapByType(TextureMap::SHEEN_ROUGHNESS)->texture());
  }

  // Check that a model with sheen is loaded as a scene with sheen.
  {
    // Load model as a scene.
    const std::unique_ptr<Scene> scene(DecodeGltfFileToScene(
        "KhronosSampleModels/SheenCloth/glTF/SheenCloth.gltf"));
    EXPECT_EQ(scene->GetMaterialLibrary().NumMaterials(), 1);
    const Material& material = *scene->GetMaterialLibrary().GetMaterial(0);

    // Check that material has sheen.
    EXPECT_TRUE(material.HasSheen());

    // Check that sheen color and roughness factors are present.
    EXPECT_EQ(material.GetSheenColorFactor(), Vector3f(1.f, 1.f, 1.f));
    EXPECT_EQ(material.GetSheenRoughnessFactor(), 1.f);

    // Check that sheen color and roughness textures are present.
    EXPECT_NE(material.GetTextureMapByType(TextureMap::SHEEN_COLOR), nullptr);
    EXPECT_NE(material.GetTextureMapByType(TextureMap::SHEEN_ROUGHNESS),
              nullptr);

    // Check that sheen color and roughness textures are shared.
    EXPECT_EQ(
        material.GetTextureMapByType(TextureMap::SHEEN_COLOR)->texture(),
        material.GetTextureMapByType(TextureMap::SHEEN_ROUGHNESS)->texture());
  }
}

TEST(GltfDecoderTest, PbrNextExtensions) {
  // Check that a model with no material extensions is loaded correctly.
  {
    const std::unique_ptr<Scene> scene(
        DecodeGltfFileToScene("Box/glTF/Box.gltf"));
    EXPECT_EQ(scene->GetMaterialLibrary().NumMaterials(), 1);
    const Material& m = *scene->GetMaterialLibrary().GetMaterial(0);

    // Check that material has no extensions.
    EXPECT_FALSE(m.HasSheen());
    EXPECT_FALSE(m.HasTransmission());
    EXPECT_FALSE(m.HasClearcoat());
    EXPECT_FALSE(m.HasVolume());
    EXPECT_FALSE(m.HasIor());
    EXPECT_FALSE(m.HasSpecular());
  }

  // Check that a model with material extensions is loaded correctly.
  {
    const std::unique_ptr<Mesh> mesh(
        DecodeGltfFile("pbr_next/sphere/glTF/sphere.gltf"));
    EXPECT_NE(mesh, nullptr);
    const Material& m = *mesh->GetMaterialLibrary().GetMaterial(0);

    // Check that material has extensions.
    EXPECT_TRUE(m.HasSheen());
    EXPECT_TRUE(m.HasTransmission());
    EXPECT_TRUE(m.HasClearcoat());
    EXPECT_TRUE(m.HasVolume());
    EXPECT_TRUE(m.HasIor());
    EXPECT_TRUE(m.HasSpecular());

    // Check that material has correct extension properties.
    EXPECT_EQ(m.GetSheenColorFactor(), Vector3f(1.0f, 0.329f, 0.1f));
    EXPECT_EQ(m.GetSheenRoughnessFactor(), 0.8f);
    EXPECT_EQ(m.GetTransmissionFactor(), 0.75f);
    EXPECT_EQ(m.GetClearcoatFactor(), 0.95f);
    EXPECT_EQ(m.GetClearcoatRoughnessFactor(), 0.03f);
    EXPECT_EQ(m.GetAttenuationColor(), Vector3f(0.921f, 0.640f, 0.064f));
    EXPECT_EQ(m.GetAttenuationDistance(), 0.155f);
    EXPECT_EQ(m.GetThicknessFactor(), 2.27f);
    EXPECT_EQ(m.GetIor(), 1.55f);
    EXPECT_EQ(m.GetSpecularFactor(), 0.3f);
    EXPECT_EQ(m.GetSpecularColorFactor(), Vector3f(0.212f, 0.521f, 0.051f));

    // Check that material has all extension textures.
    EXPECT_NE(m.GetTextureMapByType(TextureMap::SHEEN_COLOR), nullptr);
    EXPECT_NE(m.GetTextureMapByType(TextureMap::SHEEN_ROUGHNESS), nullptr);
    EXPECT_NE(m.GetTextureMapByType(TextureMap::TRANSMISSION), nullptr);
    EXPECT_NE(m.GetTextureMapByType(TextureMap::CLEARCOAT), nullptr);
    EXPECT_NE(m.GetTextureMapByType(TextureMap::CLEARCOAT_ROUGHNESS), nullptr);
    EXPECT_NE(m.GetTextureMapByType(TextureMap::CLEARCOAT_NORMAL), nullptr);
    EXPECT_NE(m.GetTextureMapByType(TextureMap::THICKNESS), nullptr);
    EXPECT_NE(m.GetTextureMapByType(TextureMap::SPECULAR), nullptr);
    EXPECT_NE(m.GetTextureMapByType(TextureMap::SPECULAR_COLOR), nullptr);
  }
}

TEST(GltfDecoderTest, TextureTransformTest) {
  const std::string filename =
      "KhronosSampleModels/TextureTransformTest/glTF/TextureTransformTest.gltf";
  const std::unique_ptr<Mesh> mesh(DecodeGltfFile(filename));
  EXPECT_EQ(mesh->GetMaterialLibrary().NumMaterials(), 9);
  std::set<int> expected_default_transforms = {4, 5, 6};
  for (int i = 0; i < 9; ++i) {
    const bool expected_default = (expected_default_transforms.count(i) != 0);
    EXPECT_EQ(TextureTransform::IsDefault(mesh->GetMaterialLibrary()
                                              .GetMaterial(i)
                                              ->GetTextureMapByIndex(0)
                                              ->texture_transform()),
              expected_default);
  }

  const std::unique_ptr<Scene> scene(DecodeGltfFileToScene(filename));
  EXPECT_EQ(scene->GetMaterialLibrary().NumMaterials(), 9);
  for (int i = 0; i < 6; ++i) {
    EXPECT_FALSE(TextureTransform::IsDefault(scene->GetMaterialLibrary()
                                                 .GetMaterial(i)
                                                 ->GetTextureMapByIndex(0)
                                                 ->texture_transform()));
  }
  for (int i = 6; i < 9; ++i) {
    EXPECT_TRUE(TextureTransform::IsDefault(scene->GetMaterialLibrary()
                                                .GetMaterial(i)
                                                ->GetTextureMapByIndex(0)
                                                ->texture_transform()));
  }
}

TEST(GltfDecoderTest, GlbTextureSource) {
  const std::string file_name = "KhronosSampleModels/Duck/glTF_Binary/Duck.glb";
  const std::unique_ptr<Scene> scene(DecodeGltfFileToScene(file_name));
  EXPECT_EQ(scene->NumMeshes(), 1);
  EXPECT_EQ(scene->NumMeshGroups(), 1);
  const MeshGroup& mesh_group = *scene->GetMeshGroup(MeshGroupIndex(0));
  ASSERT_EQ(mesh_group.NumMeshInstances(), 1);
  ASSERT_EQ(mesh_group.GetMeshInstance(0).material_index, 0);
  EXPECT_EQ(scene->NumNodes(), 3);
  EXPECT_EQ(scene->NumRootNodes(), 1);
  EXPECT_EQ(scene->GetMaterialLibrary().NumMaterials(), 1);
  EXPECT_EQ(scene->GetMaterialLibrary().GetMaterial(0)->NumTextureMaps(), 1);
  EXPECT_EQ(scene->NumAnimations(), 0);
  EXPECT_EQ(scene->NumSkins(), 0);
  EXPECT_EQ(scene->GetMaterialLibrary().GetTextureLibrary().NumTextures(), 1);
  const Texture* const texture =
      scene->GetMaterialLibrary().GetTextureLibrary().GetTexture(0);
  ASSERT_NE(texture, nullptr);
  const SourceImage& source_image = texture->source_image();
  EXPECT_EQ(source_image.encoded_data().size(), 16302);
  EXPECT_EQ(source_image.filename(), "");
  EXPECT_EQ(source_image.mime_type(), "image/png");
}

TEST(GltfDecoderTest, GlbTextureSourceWithImageName) {
  const std::string file_name =
      "KhronosSampleModels/Duck/glTF_Binary/DuckWithName.glb";
  const std::unique_ptr<Scene> scene(DecodeGltfFileToScene(file_name));
  EXPECT_EQ(scene->NumMeshes(), 1);
  EXPECT_EQ(scene->NumMeshGroups(), 1);
  const MeshGroup& mesh_group = *scene->GetMeshGroup(MeshGroupIndex(0));
  ASSERT_EQ(mesh_group.NumMeshInstances(), 1);
  ASSERT_EQ(mesh_group.GetMeshInstance(0).material_index, 0);
  EXPECT_EQ(scene->NumNodes(), 3);
  EXPECT_EQ(scene->NumRootNodes(), 1);
  EXPECT_EQ(scene->GetMaterialLibrary().NumMaterials(), 1);
  EXPECT_EQ(scene->GetMaterialLibrary().GetMaterial(0)->NumTextureMaps(), 1);
  EXPECT_EQ(scene->NumAnimations(), 0);
  EXPECT_EQ(scene->NumSkins(), 0);
  EXPECT_EQ(scene->GetMaterialLibrary().GetTextureLibrary().NumTextures(), 1);
  const Texture* const texture =
      scene->GetMaterialLibrary().GetTextureLibrary().GetTexture(0);
  ASSERT_NE(texture, nullptr);
  const SourceImage& source_image = texture->source_image();
  EXPECT_EQ(source_image.encoded_data().size(), 16302);
  EXPECT_EQ(source_image.filename(), "DuckyMcDuckFace.png");
  EXPECT_EQ(source_image.mime_type(), "image/png");
}

TEST(GltfDecoderTest, GltfTextureSource) {
  const std::string file_name = "KhronosSampleModels/Duck/glTF/Duck.gltf";
  const std::unique_ptr<Scene> scene(DecodeGltfFileToScene(file_name));
  EXPECT_EQ(scene->NumMeshes(), 1);
  EXPECT_EQ(scene->NumMeshGroups(), 1);
  const MeshGroup& mesh_group = *scene->GetMeshGroup(MeshGroupIndex(0));
  ASSERT_EQ(mesh_group.NumMeshInstances(), 1);
  ASSERT_EQ(mesh_group.GetMeshInstance(0).material_index, 0);
  EXPECT_EQ(scene->NumNodes(), 3);
  EXPECT_EQ(scene->NumRootNodes(), 1);
  EXPECT_EQ(scene->GetMaterialLibrary().NumMaterials(), 1);
  EXPECT_EQ(scene->GetMaterialLibrary().GetMaterial(0)->NumTextureMaps(), 1);
  EXPECT_EQ(scene->NumAnimations(), 0);
  EXPECT_EQ(scene->NumSkins(), 0);
  EXPECT_EQ(scene->GetMaterialLibrary().GetTextureLibrary().NumTextures(), 1);
  const Texture* const texture =
      scene->GetMaterialLibrary().GetTextureLibrary().GetTexture(0);
  ASSERT_NE(texture, nullptr);
  const SourceImage& source_image = texture->source_image();
  EXPECT_EQ(source_image.encoded_data().size(), 0);
  EXPECT_FALSE(source_image.filename().empty());
  EXPECT_EQ(source_image.mime_type(), "");
}

TEST(GltfDecoderTest, GltfDecodeWithDraco) {
  // Tests that we can decode a glTF containing Draco compressed geometry.
  const std::string file_name = "Box/glTF_Binary/Box.glb";
  const std::string file_name_with_draco = "Box/glTF_Binary/Box_Draco.glb";
  const std::unique_ptr<Scene> scene(DecodeGltfFileToScene(file_name));
  const std::unique_ptr<Scene> scene_draco(
      DecodeGltfFileToScene(file_name_with_draco));
  ASSERT_NE(scene, nullptr);
  ASSERT_NE(scene_draco, nullptr);
  EXPECT_EQ(scene->NumMeshes(), scene_draco->NumMeshes());
  EXPECT_EQ(scene->NumMeshGroups(), scene_draco->NumMeshGroups());
  EXPECT_EQ(scene->NumNodes(), scene_draco->NumNodes());
  EXPECT_EQ(scene->NumRootNodes(), scene_draco->NumRootNodes());
  EXPECT_EQ(scene->GetMaterialLibrary().NumMaterials(),
            scene_draco->GetMaterialLibrary().NumMaterials());
  EXPECT_EQ(scene->NumAnimations(), scene_draco->NumAnimations());
  EXPECT_EQ(scene->NumSkins(), scene_draco->NumSkins());

  EXPECT_EQ(scene->NumMeshes(), 1);
  EXPECT_EQ(scene->GetMesh(draco::MeshIndex(0)).num_faces(),
            scene_draco->GetMesh(draco::MeshIndex(0)).num_faces());
}

TEST(GltfDecoderTest, TestAnimationNames) {
  const std::string file_name = "InterpolationTest/glTF/InterpolationTest.gltf";
  const std::unique_ptr<Scene> scene(DecodeGltfFileToScene(file_name));
  ASSERT_NE(scene, nullptr);

  EXPECT_EQ(scene->NumAnimations(), 9);

  const std::vector<std::string> animation_names{
      "Step Scale",           "Linear Scale",
      "CubicSpline Scale",    "Step Rotation",
      "CubicSpline Rotation", "Linear Rotation",
      "Step Translation",     "CubicSpline Translation",
      "Linear Translation"};
  for (int i = 0; i < scene->NumAnimations(); ++i) {
    const Animation* const anim = scene->GetAnimation(AnimationIndex(i));
    ASSERT_NE(anim, nullptr);
    ASSERT_EQ(anim->GetName(), animation_names[i]);
  }
}

TEST(GltfDecoderTest, DuplicatePrimitives) {
  const std::string file_name = "DuplicateMeshes/duplicate_meshes.gltf";
  const std::unique_ptr<Scene> scene(DecodeGltfFileToScene(file_name));
  ASSERT_NE(scene, nullptr);

  // There should be only one unique base mesh in the scene and four mesh
  // groups (instances).
  ASSERT_EQ(scene->NumMeshes(), 1);
  ASSERT_EQ(scene->NumMeshGroups(), 4);

  // There should be two materials used by the instances.
  ASSERT_EQ(scene->GetMaterialLibrary().NumMaterials(), 2);
}

TEST(GltfDecoderTest, SimpleSkin) {
  // This is a simple skin example from glTF tutorial.
  const std::string file_name = "simple_skin.gltf";
  const std::unique_ptr<Scene> scene(DecodeGltfFileToScene(file_name));
  ASSERT_NE(scene, nullptr);

  // Check scene size.
  ASSERT_EQ(scene->NumMeshes(), 1);
  ASSERT_EQ(scene->NumMeshGroups(), 1);
  ASSERT_EQ(scene->GetMeshGroup(draco::MeshGroupIndex(0))->NumMeshInstances(),
            1);
  ASSERT_EQ(scene->NumNodes(), 3);
  ASSERT_EQ(scene->NumRootNodes(), 1);
  ASSERT_EQ(scene->GetMaterialLibrary().NumMaterials(), 1);
  ASSERT_EQ(scene->NumAnimations(), 1);
  ASSERT_EQ(scene->NumSkins(), 1);

  // Check animation size.
  const Animation* const animation = scene->GetAnimation(AnimationIndex(0));
  ASSERT_NE(animation, nullptr);
  ASSERT_EQ(animation->NumSamplers(), 1);
  ASSERT_EQ(animation->NumChannels(), 1);
  ASSERT_EQ(animation->NumNodeAnimationData(), 2);

  // Check animation sampler.
  const AnimationSampler* const sampler = animation->GetSampler(0);
  ASSERT_NE(sampler, nullptr);
  ASSERT_EQ(sampler->input_index, 0);
  ASSERT_EQ(sampler->interpolation_type,
            AnimationSampler::SamplerInterpolation::LINEAR);
  ASSERT_EQ(sampler->output_index, 1);

  // Check animation channel.
  const AnimationChannel* const channel = animation->GetChannel(0);
  ASSERT_NE(channel, nullptr);
  ASSERT_EQ(channel->sampler_index, 0);
  ASSERT_EQ(channel->target_index, 2);
  ASSERT_EQ(channel->transformation_type,
            AnimationChannel::ChannelTransformation::ROTATION);

  // Check the first node animation data.
  {
    const NodeAnimationData* const node_animation =
        animation->GetNodeAnimationData(0);
    ASSERT_EQ(node_animation->ComponentSize(), 4);
    ASSERT_EQ(node_animation->NumComponents(), 1);
    ASSERT_EQ(node_animation->count(), 12);
    ASSERT_EQ(node_animation->type(), NodeAnimationData::Type::SCALAR);
    ASSERT_FALSE(node_animation->normalized());
    const std::vector<float>& node_animation_data = *node_animation->GetData();
    const std::vector<float> expected_node_animation_data{
        0.0f, 0.5f, 1.0f, 1.5f, 2.0f, 2.5f, 3.0f, 3.5f, 4.0f, 4.5f, 5.0f, 5.5f};
    ASSERT_EQ(node_animation_data, expected_node_animation_data);
  }

  // Check the second node animation data.
  {
    const NodeAnimationData* const node_animation =
        animation->GetNodeAnimationData(1);
    ASSERT_EQ(node_animation->ComponentSize(), 4);
    ASSERT_EQ(node_animation->NumComponents(), 4);
    ASSERT_EQ(node_animation->count(), 12);
    ASSERT_EQ(node_animation->type(), NodeAnimationData::Type::VEC4);
    ASSERT_FALSE(node_animation->normalized());
    const std::vector<float>& node_animation_data = *node_animation->GetData();
    // clang-format off
    const std::vector<float> expected_node_animation_data{
      0.000f,  0.000f,  0.000f,  1.000f,
      0.000f,  0.000f,  0.383f,  0.924f,
      0.000f,  0.000f,  0.707f,  0.707f,
      0.000f,  0.000f,  0.707f,  0.707f,
      0.000f,  0.000f,  0.383f,  0.924f,
      0.000f,  0.000f,  0.000f,  1.000f,
      0.000f,  0.000f,  0.000f,  1.000f,
      0.000f,  0.000f, -0.383f,  0.924f,
      0.000f,  0.000f, -0.707f,  0.707f,
      0.000f,  0.000f, -0.707f,  0.707f,
      0.000f,  0.000f, -0.383f,  0.924f,
      0.000f,  0.000f,  0.000f,  1.000f};
    // clang-format on
    ASSERT_EQ(node_animation_data, expected_node_animation_data);
  }

  // Check skin.
  const Skin* const skin = scene->GetSkin(SkinIndex(0));
  ASSERT_NE(skin, nullptr);
  ASSERT_EQ(skin->NumJoints(), 2);
  ASSERT_EQ(skin->GetJointRoot(), kInvalidSceneNodeIndex);
  ASSERT_EQ(skin->GetJoint(0), SceneNodeIndex(1));
  ASSERT_EQ(skin->GetJoint(1), SceneNodeIndex(2));

  // Check inverse bind matrices.
  const NodeAnimationData& bind_matrices = skin->GetInverseBindMatrices();
  ASSERT_EQ(bind_matrices.type(), NodeAnimationData::Type::MAT4);
  ASSERT_EQ(bind_matrices.count(), 2);
  ASSERT_EQ(bind_matrices.normalized(), false);
  ASSERT_NE(bind_matrices.GetData(), nullptr);
  const std::vector<float>& bind_matrices_data = *bind_matrices.GetData();
  // clang-format off
  const std::vector<float> expected_bind_matrices_data{
      // First matrix.
      1.0f,  0.0f,  0.0f,  0.0f,
      0.0f,  1.0f,  0.0f,  0.0f,
      0.0f,  0.0f,  1.0f,  0.0f,
     -0.5f, -1.0f,  0.0f,  1.0f,
      // Second matrix.
      1.0f,  0.0f,  0.0f,  0.0f,
      0.0f,  1.0f,  0.0f,  0.0f,
      0.0f,  0.0f,  1.0f,  0.0f,
     -0.5f, -1.0f,  0.0f,  1.0f};
  // clang-format on
  ASSERT_EQ(bind_matrices_data, expected_bind_matrices_data);

  // Check mesh size. Note that vertex deduplication is disabled for meshes with
  // joints to avoid removing triangles that are initially degenerate but are
  // inflated as animation progresses.
  const Mesh& mesh = scene->GetMesh(MeshIndex(0));
  ASSERT_EQ(mesh.num_faces(), 8);
  ASSERT_EQ(mesh.num_points(), 24);
  ASSERT_EQ(mesh.num_attributes(), 3);

  // Check vertex joint indices.
  const PointAttribute* const joints_att =
      mesh.GetNamedAttribute(GeometryAttribute::JOINTS);
  ASSERT_NE(joints_att, nullptr);
  ASSERT_EQ(joints_att->data_type(), DT_UINT16);
  ASSERT_EQ(joints_att->num_components(), 4);
  ASSERT_EQ(joints_att->size(), 24);
  // clang-format off
  const std::array<uint16_t, 4 * 24> expected_joints = {
      // Each vertex is associated with four joints.
      0, 1, 0, 0,
      0, 1, 0, 0,
      0, 1, 0, 0,
      0, 1, 0, 0,
      0, 1, 0, 0,
      0, 1, 0, 0,
      0, 1, 0, 0,
      0, 1, 0, 0,
      0, 1, 0, 0,
      0, 1, 0, 0,
      0, 0, 0, 0,
      0, 0, 0, 0,
      0, 0, 0, 0,
      0, 0, 0, 0,
      0, 0, 0, 0,
      0, 0, 0, 0,
      0, 0, 0, 0,
      0, 0, 0, 0,
      0, 0, 0, 0,
      0, 0, 0, 0,
      0, 0, 0, 0,
      0, 0, 0, 0,
      0, 0, 0, 0,
      0, 0, 0, 0};
  // clang-format on
  std::array<uint16_t, 4 * 24> joints;
  for (draco::PointIndex pi(0); pi < mesh.num_points(); ++pi) {
    joints_att->GetMappedValue(pi, &joints[4 * pi.value()]);
  }
  ASSERT_EQ(joints, expected_joints);

  // Check vertex joint weights.
  const PointAttribute* const weights_att =
      mesh.GetNamedAttribute(GeometryAttribute::WEIGHTS);
  ASSERT_NE(weights_att, nullptr);
  ASSERT_EQ(weights_att->data_type(), DT_FLOAT32);
  ASSERT_EQ(weights_att->num_components(), 4);
  ASSERT_EQ(weights_att->size(), 24);
  // clang-format off
  const std::array<float, 4 * 24> expected_weights = {
      // Each vertex has four joint weights.
      1.00f, 0.00f, 0.00f, 0.00f,
      1.00f, 0.00f, 0.00f, 0.00f,
      0.75f, 0.25f, 0.00f, 0.00f,
      0.75f, 0.25f, 0.00f, 0.00f,
      0.50f, 0.50f, 0.00f, 0.00f,
      0.50f, 0.50f, 0.00f, 0.00f,
      0.25f, 0.75f, 0.00f, 0.00f,
      0.25f, 0.75f, 0.00f, 0.00f,
      0.00f, 1.00f, 0.00f, 0.00f,
      0.00f, 1.00f, 0.00f, 0.00f,
      0.00f, 0.00f, 0.00f, 0.00f,
      0.00f, 0.00f, 0.00f, 0.00f,
      0.00f, 0.00f, 0.00f, 0.00f,
      0.00f, 0.00f, 0.00f, 0.00f,
      0.00f, 0.00f, 0.00f, 0.00f,
      0.00f, 0.00f, 0.00f, 0.00f,
      0.00f, 0.00f, 0.00f, 0.00f,
      0.00f, 0.00f, 0.00f, 0.00f,
      0.00f, 0.00f, 0.00f, 0.00f,
      0.00f, 0.00f, 0.00f, 0.00f,
      0.00f, 0.00f, 0.00f, 0.00f,
      0.00f, 0.00f, 0.00f, 0.00f,
      0.00f, 0.00f, 0.00f, 0.00f,
      0.00f, 0.00f, 0.00f, 0.00f};
  // clang-format on
  std::array<float, 4 * 24> weights;
  for (draco::PointIndex pi(0); pi < mesh.num_points(); ++pi) {
    weights_att->GetMappedValue(pi, &weights[4 * pi.value()]);
  }
  ASSERT_EQ(weights, expected_weights);
}

TEST(GltfDecoderTest, DecodeMeshWithImplicitPrimitiveIndices) {
  // Check that glTF primitives with implicit indices can be loaded as a mesh.
  const std::string file_name = "Fox/glTF/Fox.gltf";
  const std::unique_ptr<Mesh> mesh(DecodeGltfFile(file_name));
  ASSERT_NE(mesh, nullptr);
  ASSERT_EQ(mesh->num_faces(), 576);
}

TEST(GltfDecoderTest, DecodeSceneWithImplicitPrimitiveIndices) {
  // Check that glTF primitives with implicit indices can be loaded as a scene.
  const std::string file_name = "Fox/glTF/Fox.gltf";
  const std::unique_ptr<Scene> scene(DecodeGltfFileToScene(file_name));
  ASSERT_NE(scene, nullptr);
  ASSERT_EQ(scene->NumMeshes(), 1);
  ASSERT_EQ(scene->GetMesh(MeshIndex(0)).num_faces(), 576);
}

TEST(GltfDecoderTest, DecodeFromBufferToMesh) {
  // Checks that a mesh can be decoded from buffer in GLB format.
  // Read GLB file contents into a buffer.
  const std::string file_name = "KhronosSampleModels/Duck/glTF_Binary/Duck.glb";
  const std::string file_path = GetTestFileFullPath(file_name);
  std::vector<char> file_data;
  ASSERT_TRUE(ReadFileToBuffer(file_path, &file_data));
  DecoderBuffer buffer;
  buffer.Init(file_data.data(), file_data.size());

  // Decode mesh from buffer.
  GltfDecoder decoder;
  DRACO_ASSIGN_OR_ASSERT(auto mesh, decoder.DecodeFromBuffer(&buffer));
  ASSERT_NE(mesh, nullptr);

  // Decode mesh from GLB file.
  const std::unique_ptr<Mesh> expected_mesh(DecodeGltfFile(file_name));
  ASSERT_NE(expected_mesh, nullptr);

  // Check that meshes decoded from the buffer and from GLB file are equivalent.
  MeshAreEquivalent eq;
  ASSERT_TRUE(eq(*mesh, *expected_mesh));
}

TEST(GltfDecoderTest, DecodeGraph) {
  // Checks that we can decode a scene with a general graph structure where a
  // node has multiple parents.
  // The input model has one root node, 4 children nodes that all point to a
  // single node that contains the cube mesh.
  const std::string file_name = "CubeScaledInstances/glTF/cube_att.gltf";
  const std::string file_path = GetTestFileFullPath(file_name);

  // First decode the scene into a tree-graph.
  draco::GltfDecoder dec_tree;
  DRACO_ASSIGN_OR_ASSERT(auto scene_tree,
                         dec_tree.DecodeFromFileToScene(file_path));
  // We expect to have 9 nodes with 4 mesh instances. The leaf node with the
  // cube is duplicated 4 times, once for each instance.
  ASSERT_EQ(scene_tree->NumNodes(), 9);
  auto instances_tree = draco::SceneUtils::ComputeAllInstances(*scene_tree);
  ASSERT_EQ(instances_tree.size(), 4);

  // Decode the scene into a scene-graph.
  draco::GltfDecoder dec_graph;
  dec_graph.SetSceneGraphMode(draco::GltfDecoder::GltfSceneGraphMode::DAG);
  DRACO_ASSIGN_OR_ASSERT(auto scene_graph,
                         dec_graph.DecodeFromFileToScene(file_path));

  // We expect to have 6 nodes with 4 mesh instances. The leaf node is shared
  // for all mesh instances.
  ASSERT_EQ(scene_graph->NumNodes(), 6);
  auto instances_graph = draco::SceneUtils::ComputeAllInstances(*scene_graph);
  ASSERT_EQ(instances_graph.size(), 4);

  // Check that all instances share the same scene node.
  for (draco::MeshInstanceIndex mii(1); mii < 4; ++mii) {
    ASSERT_EQ(instances_graph[mii - 1].scene_node_index,
              instances_graph[mii].scene_node_index);
  }
}

TEST(GltfDecoderTest, CorrectVolumeThicknessFactor) {
  // Checks that when a model is decoded as draco::Mesh the PBR material volume
  // thickness factor is corrected according to geometry transformation scale in
  // the scene graph.
  constexpr float kDragonScale = 0.25f;
  constexpr float kDragonVolumeThickness = 2.27f;

  // Read model as draco::Scene and check dragon mesh transformation scale and
  // its PBR material volume thickness factor.
  const std::unique_ptr<draco::Scene> scene = draco::ReadSceneFromTestFile(
      "KhronosSampleModels/DragonAttenuation/glTF/DragonAttenuation.gltf");
  ASSERT_NE(scene, nullptr);
  auto instances = draco::SceneUtils::ComputeAllInstances(*scene);
  ASSERT_EQ(instances.size(), 2);
  ASSERT_EQ(instances[MeshInstanceIndex(1)].transform.col(0).norm(),
            kDragonScale);
  ASSERT_EQ(scene->GetMaterialLibrary().GetMaterial(1)->GetThicknessFactor(),
            kDragonVolumeThickness);

  // Read model as draco::Mesh and check corrected volume thickness factor.
  const std::unique_ptr<draco::Mesh> mesh = draco::ReadMeshFromTestFile(
      "KhronosSampleModels/DragonAttenuation/glTF/DragonAttenuation.gltf");
  ASSERT_NE(mesh, nullptr);
  ASSERT_EQ(mesh->GetMaterialLibrary().GetMaterial(1)->GetThicknessFactor(),
            kDragonScale * kDragonVolumeThickness);
}

TEST(GltfDecoderTest, DecodeLightsIntoMesh) {
  // Checks that a model with lights can be decoded into draco::Mesh with the
  // lights discarded.
  const std::string file_name = "sphere_lights.gltf";
  const std::unique_ptr<Mesh> mesh(DecodeGltfFile(file_name));
  ASSERT_NE(mesh, nullptr);
  ASSERT_EQ(mesh->num_faces(), 224);
}

TEST(GltfDecoderTest, DecodeLightsIntoScene) {
  // Checks that a model with lights can be decoded into draco::Scene.
  const std::string file_name = "sphere_lights.gltf";
  const std::unique_ptr<Scene> scene(DecodeGltfFileToScene(file_name));
  ASSERT_NE(scene, nullptr);
  ASSERT_EQ(scene->NumLights(), 4);

  // Check spot light with all properties specified.
  Light& light = *scene->GetLight(LightIndex(0));
  ASSERT_EQ(light.GetName(), "Blue Lightsaber");
  ASSERT_EQ(light.GetColor(), draco::Vector3f(0.72f, 0.71f, 1.00f));
  ASSERT_EQ(light.GetIntensity(), 3.0);
  ASSERT_EQ(light.GetType(), draco::Light::SPOT);
  ASSERT_EQ(light.GetRange(), 100);
  ASSERT_EQ(light.GetInnerConeAngle(), 0.2);
  ASSERT_EQ(light.GetOuterConeAngle(), 0.8);

  // Check point light with all properties specified.
  light = *scene->GetLight(LightIndex(1));
  ASSERT_EQ(light.GetName(), "The Star of Earendil");
  ASSERT_EQ(light.GetColor(), draco::Vector3f(0.90f, 0.97f, 1.0f));
  ASSERT_EQ(light.GetIntensity(), 5.0);
  ASSERT_EQ(light.GetType(), draco::Light::POINT);
  ASSERT_EQ(light.GetRange(), 1000);
  ASSERT_EQ(light.GetInnerConeAngle(), 0.0);
  ASSERT_NEAR(light.GetOuterConeAngle(), DRACO_PI / 4.0f, 1e-8);

  // Check directional light with some properties specified.
  light = *scene->GetLight(LightIndex(2));
  ASSERT_EQ(light.GetName(), "Arc Reactor");
  ASSERT_EQ(light.GetColor(), draco::Vector3f(0.9f, 0.9, 0.9f));
  ASSERT_EQ(light.GetIntensity(), 1.0);
  ASSERT_EQ(light.GetType(), draco::Light::DIRECTIONAL);
  ASSERT_EQ(light.GetRange(), 200.0);

  // Check spot light with no properties specified.
  light = *scene->GetLight(LightIndex(3));
  ASSERT_EQ(light.GetName(), "");
  ASSERT_EQ(light.GetColor(), draco::Vector3f(1.0f, 1.0f, 1.0f));
  ASSERT_EQ(light.GetIntensity(), 1.0);
  ASSERT_EQ(light.GetType(), draco::Light::SPOT);
  ASSERT_EQ(light.GetRange(), std::numeric_limits<float>::max());
  ASSERT_EQ(light.GetInnerConeAngle(), 0.0);
  ASSERT_NEAR(light.GetOuterConeAngle(), DRACO_PI / 4.0f, 1e-8);

  // Check that lights are referenced by the scene nodes.
  ASSERT_EQ(scene->GetNode(SceneNodeIndex(0))->GetLightIndex(),
            kInvalidLightIndex);
  ASSERT_EQ(scene->GetNode(SceneNodeIndex(1))->GetLightIndex(), LightIndex(0));
  ASSERT_EQ(scene->GetNode(SceneNodeIndex(2))->GetLightIndex(), LightIndex(2));
  ASSERT_EQ(scene->GetNode(SceneNodeIndex(3))->GetLightIndex(), LightIndex(3));
  ASSERT_EQ(scene->GetNode(SceneNodeIndex(4))->GetLightIndex(), LightIndex(1));
}

TEST(GltfDecoderTest, DecodeToyCar) {
  // Checks that a model with KHR_materials_clearcoat and KHR_materials_sheen
  // can be decoded into draco::Scene.
  const std::string file_name = "KhronosSampleModels/ToyCar/glTF/ToyCar.gltf";
  const std::unique_ptr<Scene> scene(DecodeGltfFileToScene(file_name));
  ASSERT_NE(scene, nullptr);
  // We just want to make sure it decodes successfully.
  // ToyCar has many meshes and materials.
  ASSERT_GT(scene->NumMeshes(), 0);
  ASSERT_GT(scene->GetMaterialLibrary().NumMaterials(), 0);
}

TEST(GltfDecoderTest, DecodePbrNextSphere) {
  // Checks that a model with KHR_materials_ior and KHR_materials_specular
  // can be decoded into draco::Scene.
  const std::string file_name = "pbr_next/sphere/glTF/sphere.gltf";
  const std::unique_ptr<Scene> scene(DecodeGltfFileToScene(file_name));
  ASSERT_NE(scene, nullptr);
  // We just want to make sure it decodes successfully.
  ASSERT_GT(scene->NumMeshes(), 0);
  ASSERT_GT(scene->GetMaterialLibrary().NumMaterials(), 0);
}

TEST(GltfDecoderTest, DecodeLightsRequiredIntoScene) {
  // Checks that a model with KHR_lights_punctual in extensionsRequired can be
  // decoded into draco::Scene.
  const std::string file_name = "sphere_lights_required.gltf";
  const std::unique_ptr<Scene> scene(DecodeGltfFileToScene(file_name));
  ASSERT_NE(scene, nullptr);
  ASSERT_EQ(scene->NumLights(), 4);
}

TEST(GltfDecoderTest, DecodeEmissiveStrength) {
  // Checks that a model with KHR_materials_emissive_strength can be decoded.
  const std::string file_name = "sphere_emissive_strength.gltf";
  const std::unique_ptr<Scene> scene(DecodeGltfFileToScene(file_name));
  ASSERT_NE(scene, nullptr);
  ASSERT_GT(scene->GetMaterialLibrary().NumMaterials(), 0);
  const Material* const material = scene->GetMaterialLibrary().GetMaterial(0);
  ASSERT_NE(material, nullptr);
  ASSERT_TRUE(material->HasEmissiveStrength());
  ASSERT_EQ(material->GetEmissiveStrength(), 5.0f);
}

TEST(GltfDecoderTest, DecodeInteractivityIntoScene) {
  // Checks that a model with KHR_interactivity glTF extension (shortened for
  // testing of pass-through support) can be decoded into draco::Scene.
  const std::string file_name = "sphere_interactivity.gltf";
  const std::unique_ptr<Scene> scene(DecodeGltfFileToScene(file_name));
  ASSERT_NE(scene, nullptr);
  const auto& extensions = scene->GetPassThroughExtensions();

  // Check KHR_interactivity extension.
  ASSERT_EQ(extensions.begin()->first, "KHR_interactivity");
  const auto& interactivity = extensions.begin()->second;

  // The presence of named entities.
  ASSERT_EQ(interactivity.GetType(), JsonEntity::OBJECT);
  ASSERT_EQ(interactivity.GetObject().size(), 4);
  ASSERT_NE(interactivity.GetObjectEntityByName("types"), nullptr);
  ASSERT_NE(interactivity.GetObjectEntityByName("variables"), nullptr);
  ASSERT_NE(interactivity.GetObjectEntityByName("customEvents"), nullptr);
  ASSERT_NE(interactivity.GetObjectEntityByName("nodes"), nullptr);

  // Individual entity contents.
  {
    const auto& entity = *interactivity.GetObjectEntityByName("types");
    ASSERT_EQ(entity.GetType(), JsonEntity::ARRAY);
    ASSERT_EQ(entity.GetArray().size(), 2);
    {
      const auto& object = entity.GetArray()[0];
      ASSERT_EQ(object.GetType(), JsonEntity::OBJECT);
      const auto* signature = object.GetObjectEntityByName("signature");
      ASSERT_NE(signature, nullptr);
      ASSERT_EQ(signature->GetString(), "bool");
    }
    {
      const auto& object = entity.GetArray()[1];
      ASSERT_EQ(object.GetType(), JsonEntity::OBJECT);
      const auto* signature = object.GetObjectEntityByName("signature");
      ASSERT_NE(signature, nullptr);
      ASSERT_EQ(signature->GetString(), "int");
    }
  }
  {
    const auto& entity = *interactivity.GetObjectEntityByName("variables");
    ASSERT_EQ(entity.GetType(), JsonEntity::ARRAY);
    ASSERT_EQ(entity.GetArray().size(), 2);
    {
      const auto& object = entity.GetArray()[0];
      ASSERT_EQ(object.GetType(), JsonEntity::OBJECT);
      const auto* id = object.GetObjectEntityByName("id");
      ASSERT_NE(id, nullptr);
      ASSERT_EQ(id->GetString(), "AnimatedCrane_water_show0");
      const auto* type = object.GetObjectEntityByName("type");
      ASSERT_NE(type, nullptr);
      ASSERT_EQ(type->GetInteger(), 0);
      const auto* value = object.GetObjectEntityByName("value");
      ASSERT_NE(value, nullptr);
      ASSERT_EQ(value->GetType(), JsonEntity::BOOLEAN);
      ASSERT_EQ(value->GetBoolean(), false);
    }
    {
      const auto& object = entity.GetArray()[1];
      ASSERT_EQ(object.GetType(), JsonEntity::OBJECT);
      const auto* id = object.GetObjectEntityByName("id");
      ASSERT_NE(id, nullptr);
      ASSERT_EQ(id->GetString(), "group0");
      const auto* type = object.GetObjectEntityByName("type");
      ASSERT_NE(type, nullptr);
      ASSERT_EQ(type->GetInteger(), 0);
      const auto* value = object.GetObjectEntityByName("value");
      ASSERT_NE(value, nullptr);
      ASSERT_EQ(value->GetType(), JsonEntity::BOOLEAN);
      ASSERT_EQ(value->GetBoolean(), false);
    }
  }
  {
    const auto& entity = *interactivity.GetObjectEntityByName("customEvents");
    ASSERT_EQ(entity.GetType(), JsonEntity::ARRAY);
    ASSERT_EQ(entity.GetArray().size(), 2);
    {
      const auto& object = entity.GetArray()[0];
      ASSERT_EQ(object.GetType(), JsonEntity::OBJECT);
      const auto* id = object.GetObjectEntityByName("id");
      ASSERT_NE(id, nullptr);
      ASSERT_EQ(id->GetString(), "openUrl");
      const auto* values = object.GetObjectEntityByName("values");
      ASSERT_NE(values, nullptr);
      ASSERT_EQ(values->GetType(), JsonEntity::ARRAY);
      ASSERT_EQ(values->GetArray().size(), 1);
      {
        const auto& entry = values->GetArray()[0];
        ASSERT_EQ(entry.GetType(), JsonEntity::OBJECT);
        const auto* entry_id = entry.GetObjectEntityByName("id");
        ASSERT_NE(entry_id, nullptr);
        ASSERT_EQ(entry_id->GetString(), "url");
        const auto* type = entry.GetObjectEntityByName("type");
        ASSERT_NE(type, nullptr);
        ASSERT_EQ(type->GetInteger(), 7);
      }
    }
    {
      const auto& object = entity.GetArray()[1];
      ASSERT_EQ(object.GetType(), JsonEntity::OBJECT);
      const auto* id = object.GetObjectEntityByName("id");
      ASSERT_NE(id, nullptr);
      ASSERT_EQ(id->GetString(), "RotatingCrane_rotate0");
      const auto* values = object.GetObjectEntityByName("values");
      if (values != nullptr) {
        ASSERT_EQ(values->GetType(), JsonEntity::ARRAY);
        ASSERT_EQ(values->GetArray().size(), 0);
      }
    }
  }
  {
    const auto& entity = *interactivity.GetObjectEntityByName("nodes");
    ASSERT_EQ(entity.GetType(), JsonEntity::ARRAY);
    ASSERT_EQ(entity.GetArray().size(), 5);
    // Check Entry 2 (pointer index 4 in glTF is converted to index 3 in Draco).
    {
      const auto& node = entity.GetArray()[2];
      ASSERT_EQ(node.GetType(), JsonEntity::OBJECT);
      const auto* configuration = node.GetObjectEntityByName("configuration");
      ASSERT_NE(configuration, nullptr);
      ASSERT_EQ(configuration->GetType(), JsonEntity::OBJECT);
      const auto* pointer = configuration->GetObjectEntityByName("pointer");
      ASSERT_NE(pointer, nullptr);
      ASSERT_EQ(pointer->GetType(), JsonEntity::OBJECT);
      const auto* value = pointer->GetObjectEntityByName("value");
      ASSERT_NE(value, nullptr);
      ASSERT_EQ(value->GetType(), JsonEntity::ARRAY);
      ASSERT_EQ(value->GetArray().size(), 1);
      ASSERT_EQ(value->GetArray()[0].GetString(), "nodes/3/visibility");
    }
    // Check Entry 3 (nodeIndex 3 in glTF is converted to index 2 in Draco).
    {
      const auto& node = entity.GetArray()[3];
      ASSERT_EQ(node.GetType(), JsonEntity::OBJECT);
      const auto* configuration = node.GetObjectEntityByName("configuration");
      ASSERT_NE(configuration, nullptr);
      ASSERT_EQ(configuration->GetType(), JsonEntity::OBJECT);
      const auto* node_index_config =
          configuration->GetObjectEntityByName("nodeIndex");
      ASSERT_NE(node_index_config, nullptr);
      ASSERT_EQ(node_index_config->GetType(), JsonEntity::OBJECT);
      const auto* value = node_index_config->GetObjectEntityByName("value");
      ASSERT_NE(value, nullptr);
      ASSERT_EQ(value->GetType(), JsonEntity::ARRAY);
      ASSERT_EQ(value->GetArray().size(), 1);
      ASSERT_EQ(value->GetArray()[0].GetInteger(), 2);
    }
    // Check Entry 4 (pointer /nodes/1 in glTF is unchanged in Draco).
    {
      const auto& node = entity.GetArray()[4];
      ASSERT_EQ(node.GetType(), JsonEntity::OBJECT);
      const auto* configuration = node.GetObjectEntityByName("configuration");
      ASSERT_NE(configuration, nullptr);
      ASSERT_EQ(configuration->GetType(), JsonEntity::OBJECT);
      const auto* pointer = configuration->GetObjectEntityByName("pointer");
      ASSERT_NE(pointer, nullptr);
      ASSERT_EQ(pointer->GetType(), JsonEntity::OBJECT);
      const auto* value = pointer->GetObjectEntityByName("value");
      ASSERT_NE(value, nullptr);
      ASSERT_EQ(value->GetType(), JsonEntity::ARRAY);
      ASSERT_EQ(value->GetArray().size(), 1);
      ASSERT_EQ(value->GetArray()[0].GetString(), "/nodes/1/sceneMatrix");
    }
  }
}

TEST(GltfDecoderTest, MaterialsVariants) {
  // Checks that a model with KHR_materials_variants extension can be decoded.
  draco::GltfDecoder decoder;
  DRACO_ASSIGN_OR_ASSERT(auto scene,
                         decoder.DecodeFromFileToScene(GetTestFileFullPath(
                             "KhronosSampleModels/DragonAttenuation/glTF/"
                             "DragonAttenuation.gltf")));
  ASSERT_NE(scene, nullptr);
  const draco::MaterialLibrary& library = scene->GetMaterialLibrary();
  ASSERT_EQ(library.NumMaterialsVariants(), 2);
  ASSERT_EQ(library.GetMaterialsVariantName(0), "Attenuation");
  ASSERT_EQ(library.GetMaterialsVariantName(1), "Surface Color");

  // Check that the cloth mesh has no material variants.
  const draco::MeshGroup& cloth_group =
      *scene->GetMeshGroup(draco::MeshGroupIndex(0));
  ASSERT_EQ(cloth_group.GetName(), "Cloth Backdrop");
  ASSERT_EQ(cloth_group.NumMeshInstances(), 1);
  const auto& cloth_mappings =
      cloth_group.GetMeshInstance(0).materials_variants_mappings;
  ASSERT_EQ(cloth_mappings.size(), 0);

  // Check that the dragon has correct materials variants.
  const draco::MeshGroup& dragon_group =
      *scene->GetMeshGroup(draco::MeshGroupIndex(1));
  ASSERT_EQ(dragon_group.GetName(), "Dragon");
  ASSERT_EQ(dragon_group.NumMeshInstances(), 1);
  const auto& dragon_mappings =
      dragon_group.GetMeshInstance(0).materials_variants_mappings;
  ASSERT_EQ(dragon_mappings.size(), 2);
  ASSERT_EQ(dragon_mappings[0].material, 1);
  ASSERT_EQ(dragon_mappings[1].material, 2);
  ASSERT_EQ(dragon_mappings[0].variants.size(), 1);
  ASSERT_EQ(dragon_mappings[1].variants.size(), 1);
  ASSERT_EQ(dragon_mappings[0].variants[0], 0);
  ASSERT_EQ(dragon_mappings[1].variants[0], 1);
}

TEST(GltfDecoderTest, DecodeMeshWithMeshFeaturesWithStructuralMetadata) {
  // Checks decoding of a simple glTF with mesh features and structural metadata
  // property table as draco::Mesh.
  const auto path = GetTestFileFullPath("BoxMeta/glTF/BoxMeta.gltf");
  GltfTestHelper::UseCase use_case;
  use_case.has_mesh_features = true;
  use_case.has_structural_metadata = true;

  draco::GltfDecoder decoder;
  DRACO_ASSIGN_OR_ASSERT(auto mesh, decoder.DecodeFromFile(path));
  ASSERT_NE(mesh, nullptr);
  GltfTestHelper::CheckBoxMetaMeshFeatures(*mesh, use_case);
  GltfTestHelper::CheckBoxMetaStructuralMetadata(*mesh, use_case);
}

TEST(GltfDecoderTest, DecodeMeshWithStructuralMetadataWithEmptyStringBuffer) {
  // Checks that the decoder correctly handles 0-length buffers. An example case
  // where this could happen is an EXT_structural_metadata extension with a
  // buffer containing an empty string.
  const auto path =
      GetTestFileFullPath("ZeroLengthBufferView/ZeroLengthBufferView.gltf");
  GltfTestHelper::UseCase use_case;
  use_case.has_mesh_features = true;
  use_case.has_structural_metadata = true;

  draco::GltfDecoder decoder;
  DRACO_ASSIGN_OR_ASSERT(auto mesh, decoder.DecodeFromFile(path));
  ASSERT_NE(mesh, nullptr);
  ASSERT_EQ(mesh->GetStructuralMetadata().NumPropertyTables(), 1);
  ASSERT_EQ(mesh->GetStructuralMetadata().GetPropertyTable(0).GetCount(), 1);
  ASSERT_EQ(mesh->GetStructuralMetadata().GetPropertyTable(0).NumProperties(),
            1);
  ASSERT_EQ(mesh->GetStructuralMetadata()
                .GetPropertyTable(0)
                .GetProperty(0)
                .GetData()
                .data.size(),
            0);
}

TEST(GltfDecoderTest, DecodeMeshWithMeshFeaturesWithDracoCompression) {
  // Checks decoding of a simple glTF with mesh features compressed with Draco
  // as draco::Mesh.
  const auto path = GetTestFileFullPath("BoxMetaDraco/glTF/BoxMetaDraco.gltf");
  GltfTestHelper::UseCase use_case;
  use_case.has_draco_compression = true;
  use_case.has_mesh_features = true;

  draco::GltfDecoder decoder;
  DRACO_ASSIGN_OR_ASSERT(auto mesh, decoder.DecodeFromFile(path));
  ASSERT_NE(mesh, nullptr);
  GltfTestHelper::CheckBoxMetaMeshFeatures(*mesh, use_case);
}

TEST(GltfDecoderTest, DecodeSceneWithMeshFeaturesWithStructuralMetadata) {
  // Checks decoding of a simple glTF with mesh features and structural metadata
  // property table as draco::Scene.
  const auto path = GetTestFileFullPath("BoxMeta/glTF/BoxMeta.gltf");
  GltfTestHelper::UseCase use_case;
  use_case.has_mesh_features = true;
  use_case.has_structural_metadata = true;

  draco::GltfDecoder decoder;
  DRACO_ASSIGN_OR_ASSERT(auto scene, decoder.DecodeFromFileToScene(path));
  ASSERT_NE(scene, nullptr);
  GltfTestHelper::CheckBoxMetaMeshFeatures(*scene, use_case);
  GltfTestHelper::CheckBoxMetaStructuralMetadata(*scene, use_case);
}

TEST(GltfDecoderTest, DecodeSceneWithMeshFeaturesWithDracoCompression) {
  // Checks decoding of a simple glTF with mesh features compressed with Draco
  // as draco::Scene.
  const auto path = GetTestFileFullPath("BoxMetaDraco/glTF/BoxMetaDraco.gltf");
  GltfTestHelper::UseCase use_case;
  use_case.has_draco_compression = true;
  use_case.has_mesh_features = true;

  draco::GltfDecoder decoder;
  DRACO_ASSIGN_OR_ASSERT(auto scene, decoder.DecodeFromFileToScene(path));
  ASSERT_NE(scene, nullptr);
  GltfTestHelper::CheckBoxMetaMeshFeatures(*scene, use_case);
}

TEST(GltfDecoderTest, DecodePointCloudToMesh) {
  // Checks decoding of a simple glTF with point primitives (no meshes).
  const auto path = GetTestFileFullPath(
      "SphereTwoMaterials/sphere_two_materials_point_cloud.gltf");
  draco::GltfDecoder decoder;
  DRACO_ASSIGN_OR_ASSERT(auto mesh, decoder.DecodeFromFile(path));
  ASSERT_NE(mesh, nullptr);

  // Check the point cloud has expected number of points and attributes.
  ASSERT_EQ(mesh->num_faces(), 0);
  ASSERT_EQ(mesh->num_points(), 462);

  ASSERT_EQ(mesh->NumNamedAttributes(draco::GeometryAttribute::NORMAL), 1);
  ASSERT_EQ(mesh->NumNamedAttributes(draco::GeometryAttribute::TEX_COORD), 1);
  ASSERT_EQ(mesh->NumNamedAttributes(draco::GeometryAttribute::TANGENT), 1);
  ASSERT_EQ(mesh->NumNamedAttributes(draco::GeometryAttribute::MATERIAL), 1);

  // Verify that vertex deduplication was performed
  ASSERT_LT(mesh->GetNamedAttribute(draco::GeometryAttribute::NORMAL)->size(),
            462);

  // Check the point cloud has two materials.
  ASSERT_EQ(mesh->GetNamedAttribute(draco::GeometryAttribute::MATERIAL)->size(),
            2);
}

TEST(GltfDecoderTest, DecodeMeshAndPointCloudToMesh) {
  // Checks decoding of a simple glTF with a mesh and point primitives into
  // draco::Mesh. This should fail (draco::Mesh can't support mixed primitives).
  const auto path = GetTestFileFullPath(
      "SphereTwoMaterials/sphere_two_materials_mesh_and_point_cloud.gltf");
  draco::GltfDecoder decoder;
  ASSERT_FALSE(decoder.DecodeFromFile(path).ok());
}

TEST(GltfDecoderTest, DecodePointCloudToScene) {
  // Checks decoding of a simple glTF with point primitives (no meshes) into
  // draco::Scene.
  const auto path = GetTestFileFullPath(
      "SphereTwoMaterials/sphere_two_materials_point_cloud.gltf");
  draco::GltfDecoder decoder;
  DRACO_ASSIGN_OR_ASSERT(auto scene, decoder.DecodeFromFileToScene(path));
  ASSERT_NE(scene, nullptr);

  ASSERT_EQ(scene->NumMeshes(), 2);

  // Check that each point cloud has expected number of points and attributes.
  for (draco::MeshIndex mi(0); mi < scene->NumMeshes(); ++mi) {
    const auto& mesh = scene->GetMesh(mi);
    ASSERT_EQ(mesh.num_faces(), 0);
    ASSERT_EQ(mesh.num_points(), 231);

    ASSERT_EQ(mesh.NumNamedAttributes(draco::GeometryAttribute::NORMAL), 1);
    ASSERT_EQ(mesh.NumNamedAttributes(draco::GeometryAttribute::TEX_COORD), 1);
    ASSERT_EQ(mesh.NumNamedAttributes(draco::GeometryAttribute::TANGENT), 1);
    ASSERT_EQ(mesh.NumNamedAttributes(draco::GeometryAttribute::MATERIAL), 0);
  }

  // Check the materials are properly assigned to each point cloud.
  const auto instances = draco::SceneUtils::ComputeAllInstances(*scene);
  ASSERT_EQ(instances.size(), 2);
  ASSERT_EQ(draco::SceneUtils::GetMeshInstanceMaterialIndex(
                *scene, instances[draco::MeshInstanceIndex(0)]),
            0);
  ASSERT_EQ(draco::SceneUtils::GetMeshInstanceMaterialIndex(
                *scene, instances[draco::MeshInstanceIndex(1)]),
            1);
}

TEST(GltfDecoderTest, DecodeMeshAndPointCloudToScene) {
  // Checks decoding of a simple glTF with a mesh and point primitives into
  // draco::Scene.
  const auto path = GetTestFileFullPath(
      "SphereTwoMaterials/sphere_two_materials_mesh_and_point_cloud.gltf");
  draco::GltfDecoder decoder;
  DRACO_ASSIGN_OR_ASSERT(auto scene, decoder.DecodeFromFileToScene(path));
  ASSERT_NE(scene, nullptr);

  ASSERT_EQ(scene->NumMeshes(), 2);

  // First mesh should be a real mesh while the other one should be a point
  // cloud (no faces). Otherwise, they should have the same properties.
  for (draco::MeshIndex mi(0); mi < scene->NumMeshes(); ++mi) {
    const auto& mesh = scene->GetMesh(mi);
    ASSERT_EQ(mesh.num_faces(), mi.value() == 0 ? 224 : 0);
    ASSERT_EQ(mesh.num_points(), 231);

    ASSERT_EQ(mesh.NumNamedAttributes(draco::GeometryAttribute::NORMAL), 1);
    ASSERT_EQ(mesh.NumNamedAttributes(draco::GeometryAttribute::TEX_COORD), 1);
    ASSERT_EQ(mesh.NumNamedAttributes(draco::GeometryAttribute::TANGENT), 1);
  }
}

TEST(GltfDecoderTest, TestLoadUnboundedTexCoordAttributes) {
  // Checks that additional texture coordinate attributes (TEXCOORD_2 ...
  // TEXCOORD_7) are successfully loaded and fully supported.
  auto scene = draco::ReadSceneFromTestFile("UnusedTexCoords/TexCoord2.gltf");
  ASSERT_NE(scene, nullptr);
  ASSERT_EQ(scene->GetMesh(draco::MeshIndex(0))
                .NumNamedAttributes(draco::GeometryAttribute::TEX_COORD),
            8);
}

TEST(GltfDecoderTest, TestInvertedMaterials) {
  // Checks that GltfDecoder assigns materials properly to sub-meshes when the
  // material indices are in reverse order in the input glTF.
  auto mesh = draco::ReadMeshFromTestFile("two_objects_inverse_materials.gltf");
  ASSERT_NE(mesh, nullptr);
  ASSERT_EQ(mesh->GetMaterialLibrary().NumMaterials(), 2);

  // There are two sub-meshes. A cube with 12 faces and a sphere. The cube
  // should be mapped to a "Red" material and the sphere to a "Green" material.
  ASSERT_EQ(mesh->GetMaterialLibrary().GetMaterial(0)->GetName(), "Red");
  ASSERT_EQ(mesh->GetMaterialLibrary().GetMaterial(1)->GetName(), "Green");

  // Count the number of faces for each material index in the mesh.
  std::array<int, 2> num_material_faces = {0, 0};
  const draco::PointAttribute* const mat_att =
      mesh->GetNamedAttribute(draco::GeometryAttribute::MATERIAL);
  for (draco::FaceIndex i(0); i < mesh->num_faces(); ++i) {
    const auto f = mesh->face(i);
    uint32_t mat_index = 0;
    mat_att->GetMappedValue(f[0], &mat_index);
    ASSERT_TRUE(mat_index == 0 || mat_index == 1);
    num_material_faces[mat_index]++;
  }

  // There should be 12 faces mapped to the red material (index 0), rest should
  // be mapped to the green one.
  ASSERT_EQ(num_material_faces[0], 12);
}

TEST(GltfDecoderTest, DecodePointCloudToMeshWithDeduplicationDisabled) {
  // Checks that no deduplication is performed when it is explicitly disabled.
  const auto path = GetTestFileFullPath(
      "SphereTwoMaterials/sphere_two_materials_point_cloud.gltf");
  draco::GltfDecoder decoder;
  decoder.SetDeduplicateVertices(false);
  DRACO_ASSIGN_OR_ASSERT(auto mesh, decoder.DecodeFromFile(path));
  ASSERT_NE(mesh, nullptr);

  // Check the point cloud has expected number of points and attributes.
  ASSERT_EQ(mesh->num_faces(), 0);
  ASSERT_EQ(mesh->num_points(), 462);

  // Verify that no deduplication was performed.
  ASSERT_EQ(mesh->GetNamedAttribute(draco::GeometryAttribute::NORMAL)->size(),
            462);
}

TEST(GltfDecoderTest, PreserveMaterialIndicesWithUnusedMaterials) {
  // Ensure that we can preserve material indices even when the materials are
  // unused in the glTF.

  // First decode the glTF without the material index preservation.
  const auto path = GetTestFileFullPath("UnusedMaterial/unused_materials.gltf");
  draco::GltfDecoder decoder_default;
  DRACO_ASSIGN_OR_ASSERT(auto mesh, decoder_default.DecodeFromFile(path));

  // Ensure there is no material attribute.
  ASSERT_EQ(mesh->GetNamedAttribute(draco::GeometryAttribute::MATERIAL),
            nullptr);

  // The only used material should be named "material_1" with index 0.
  ASSERT_EQ(mesh->GetMaterialLibrary().GetMaterial(0)->GetName(), "material_1");

  // Now load the same model with the material index preservation.
  draco::GltfDecoder decoder_preserve_material_indices;
  decoder_preserve_material_indices.SetPreserveMaterialIndices(true);
  DRACO_ASSIGN_OR_ASSERT(
      mesh, decoder_preserve_material_indices.DecodeFromFile(path));

  // Ensure there is a material attribute with one entry (1).
  ASSERT_NE(mesh->GetNamedAttribute(draco::GeometryAttribute::MATERIAL),
            nullptr);
  ASSERT_EQ(mesh->GetNamedAttribute(draco::GeometryAttribute::MATERIAL)->size(),
            1);
  uint32_t mat_index = 0;
  mesh->GetNamedAttribute(draco::GeometryAttribute::MATERIAL)
      ->GetMappedValue(draco::PointIndex(0), &mat_index);
  ASSERT_EQ(mat_index, 1);

  // The only used material should be named "material_1" with index 1.
  ASSERT_EQ(mesh->GetMaterialLibrary().GetMaterial(1)->GetName(), "material_1");
}

TEST(GltfDecoderTest, DecodeSceneWith16bitTextures) {
  // Checks that a scene with a 16-bit texture can be decoded. The 16-bit
  // texture is converted to 8-bit textures.
  const auto path = GetTestFileFullPath("sphere_16bit_texture.gltf");
  draco::GltfDecoder decoder;
  DRACO_ASSIGN_OR_ASSERT(auto mesh, decoder.DecodeFromFile(path));
  ASSERT_NE(mesh, nullptr);
  ASSERT_EQ(mesh->GetMaterialLibrary().GetTextureLibrary().NumTextures(), 2);
}

TEST(GltfDecoderTest, DecodeSceneWithCustomMetadataPNGs) {
  // This test checks the part of the glTF specification that reads "Any
  // colorspace information (such as ICC profiles, intents, gamma values, etc.)
  // from PNG or JPEG images MUST be ignored." So while this is more of a
  // visual test, we are checking here to make sure the file is decoded
  // properly.
  const auto path = GetTestFileFullPath(
      "TextureEncodingTestExtreme/glTF/TextureEncodingTest.gltf");
  draco::GltfDecoder decoder;
  DRACO_ASSIGN_OR_ASSERT(auto scene, decoder.DecodeFromFileToScene(path));
  ASSERT_NE(scene, nullptr);

  ASSERT_EQ(scene->NumMeshes(), 3);
  ASSERT_EQ(scene->NumNodes(), 30);
  ASSERT_EQ(scene->NumRootNodes(), 1);
  ASSERT_EQ(scene->GetMaterialLibrary().NumMaterials(), 29);
  ASSERT_EQ(scene->GetMaterialLibrary().GetTextureLibrary().NumTextures(), 18);
}

TEST(GltfDecoderTest, DecodeGltfWithTangentAndNormalMorphTargets) {
  // Checks decoding of a glTF with morph targets that contain normal and
  // tangent attributes (in addition to the position attribute).
  const std::string path = GetTestFileFullPath(
      "KhronosSampleModels/AnimatedMorphCube/glTF/AnimatedMorphCube.gltf");

  draco::GltfDecoder decoder;
  DRACO_ASSIGN_OR_ASSERT(auto scene, decoder.DecodeFromFileToScene(path));
  ASSERT_NE(scene, nullptr);

  // Ensure the normal and tangent morph target attributes are present for both
  // morph targets of the first primitive.
  const auto& mesh = scene->GetMesh(draco::MeshIndex(0));
  ASSERT_EQ(mesh.num_morph_targets(), 2);
  ASSERT_EQ(mesh.GetMorphTarget(0).num_attributes(), 3);
  ASSERT_EQ(mesh.GetMorphTarget(1).num_attributes(), 3);

  ASSERT_NE(mesh.GetMorphTarget(0).GetNamedAttribute(
                draco::GeometryAttribute::NORMAL),
            nullptr);
  ASSERT_NE(mesh.GetMorphTarget(0).GetNamedAttribute(
                draco::GeometryAttribute::TANGENT),
            nullptr);
  ASSERT_NE(mesh.GetMorphTarget(1).GetNamedAttribute(
                draco::GeometryAttribute::NORMAL),
            nullptr);
  ASSERT_NE(mesh.GetMorphTarget(1).GetNamedAttribute(
                draco::GeometryAttribute::TANGENT),
            nullptr);

  // Ensure the tangent morph target attributes have 3 components.
  ASSERT_EQ(mesh.GetMorphTarget(0)
                .GetNamedAttribute(draco::GeometryAttribute::TANGENT)
                ->num_components(),
            3);
  ASSERT_EQ(mesh.GetMorphTarget(1)
                .GetNamedAttribute(draco::GeometryAttribute::TANGENT)
                ->num_components(),
            3);
}

TEST(GltfDecoderTest, DecodeGltfWithGenericAttributesMaintainsNames) {
  // Test to make sure that GeometryAttribute::GENERIC attributes are decoded
  // with metadata that remembers their names (so that they can be encoded
  // again successfully).
  const std::string path = GetTestFileFullPath("BoxMeta/glTF/BoxMeta.gltf");

  draco::GltfDecoder decoder;
  DRACO_ASSIGN_OR_ASSERT(auto mesh, decoder.DecodeFromFile(path));
  ASSERT_NE(mesh, nullptr);

  // Get the attribute metadatas.
  draco::GeometryMetadata* metadata = mesh->metadata();
  ASSERT_NE(metadata, nullptr);
  const std::vector<std::unique_ptr<draco::AttributeMetadata>>& attr_metadatas =
      metadata->attribute_metadatas();
  ASSERT_NE(attr_metadatas.size(), 0);

  // Collect all of the application-specific names stored on attribute metadata.
  std::set<std::string> found_names;
  constexpr char kDracoMetadataGltfAttributeName[] =
      "//GLTF/ApplicationSpecificAttributeName";
  for (const std::unique_ptr<draco::AttributeMetadata>& attr_metadata :
       attr_metadatas) {
    // Grab this application-specific attribute name.
    std::string name;
    if (!attr_metadata->GetEntryString(kDracoMetadataGltfAttributeName,
                                       &name)) {
      continue;
    }

    // Make sure we're not encountering the same name twice.
    auto [unused_it, inserted] = found_names.insert(std::move(name));
    ASSERT_TRUE(inserted);
  }

  // We expect to find specific attribute names.
  ASSERT_EQ(found_names.size(), 2);
  ASSERT_NE(found_names.find("_DIRECTION"), found_names.end());
  ASSERT_NE(found_names.find("_MAGNITUDE"), found_names.end());
}

TEST(GltfDecoderTest, DecodeGltfWithAccessorOutOfBounds) {
  // Checks that the fuzzer payload (out-of-bounds glTF accessor byteOffset)
  // fails gracefully without memory corruption.
  const auto path = GetTestFileFullPath("fuzz/test_poc.gltf");
  draco::GltfDecoder decoder;
  auto maybe_scene = decoder.DecodeFromFileToScene(path);
  ASSERT_FALSE(maybe_scene.ok());
  ASSERT_EQ(maybe_scene.status().code(), draco::Status::DRACO_ERROR);
  EXPECT_NE(maybe_scene.status().error_msg_string().find(
                "Accessor reading past the end of the bufferView."),
            std::string::npos);
}

TEST(GltfDecoderTest, TestInvalidSamplerIndex) {
  const std::string file_name = "invalid_sampler_index.gltf";
  const std::string path = GetTestFileFullPath(file_name);
  GltfDecoder decoder;
  const auto maybe_scene = decoder.DecodeFromFileToScene(path);
  EXPECT_FALSE(maybe_scene.ok());
  EXPECT_EQ(maybe_scene.status().error_msg_string(),
            "Missing texture image at source index 0");
}

TEST(GltfDecoderTest, TestInvalidAccessorBounds) {
  const std::string file_name = "invalid_accessor_bounds.glb";
  const std::string path = GetTestFileFullPath(file_name);
  GltfDecoder decoder;
  const auto maybe_scene = decoder.DecodeFromFileToScene(path);
  EXPECT_FALSE(maybe_scene.ok());
  EXPECT_EQ(maybe_scene.status().error_msg_string(),
            "Accessor reading past the end of the bufferView.");
}

TEST(GltfDecoderTest, TestInvalidNodeMeshIndices) {
  const std::string file_name = "invalid_node_mesh_indices.gltf";
  const std::string path = GetTestFileFullPath(file_name);
  GltfDecoder decoder;
  const auto maybe_scene = decoder.DecodeFromFileToScene(path);
  EXPECT_FALSE(maybe_scene.ok());
  EXPECT_EQ(maybe_scene.status().error_msg_string(),
            "Node index out of bounds.");
}

TEST(GltfDecoderTest, TestInvalidVertexIndices) {
  const std::string file_name = "invalid_vertex_indices.glb";
  const std::string path = GetTestFileFullPath(file_name);
  GltfDecoder decoder;
  const auto maybe_scene = decoder.DecodeFromFileToScene(path);
  EXPECT_FALSE(maybe_scene.ok());
  EXPECT_EQ(maybe_scene.status().error_msg_string(), "Index out of bounds");
}

TEST(GltfDecoderTest, TestSignedAttributes) {
  const std::string file_name = "test_signed_attribs.gltf";
  const std::unique_ptr<Mesh> mesh(DecodeGltfFile(file_name));
  ASSERT_NE(mesh, nullptr);
  EXPECT_EQ(mesh->num_attributes(), 4) << "Unexpected number of attributes.";
  EXPECT_EQ(mesh->num_points(), 3) << "Unexpected number of vertices.";
  EXPECT_EQ(mesh->num_faces(), 1) << "Unexpected number of faces.";

  auto check_att = [&](const std::string& name, draco::DataType expected_type,
                       const std::vector<int64_t>& expected_values) {
    const int att_id = mesh->GetAttributeIdByMetadataEntry(
        "//GLTF/ApplicationSpecificAttributeName", name);
    ASSERT_NE(att_id, -1) << "Attribute " << name << " not found";
    const auto* const att = mesh->attribute(att_id);
    ASSERT_NE(att, nullptr);
    EXPECT_EQ(att->data_type(), expected_type);
    for (int i = 0; i < 3; ++i) {
      int64_t val = 0;
      switch (expected_type) {
        case draco::DT_INT8: {
          int8_t v;
          att->GetMappedValue(PointIndex(i), &v);
          val = v;
        } break;
        case draco::DT_INT16: {
          int16_t v;
          att->GetMappedValue(PointIndex(i), &v);
          val = v;
        } break;
        case draco::DT_INT32: {
          int32_t v;
          att->GetMappedValue(PointIndex(i), &v);
          val = v;
        } break;
        default:
          FAIL() << "Unexpected type";
      }
      EXPECT_EQ(val, expected_values[i])
          << "Mismatch at index " << i << " for " << name;
    }
  };

  check_att("_INT8", draco::DT_INT8, {-1, 0, 1});
  check_att("_INT16", draco::DT_INT16, {-1000, 0, 1000});
  check_att("_INT32", draco::DT_INT32, {-100000, 0, 100000});
}

}  // namespace draco
#endif  // DRACO_TRANSCODER_SUPPORTED
