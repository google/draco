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
#include "draco/io/gltf_encoder.h"

#include <cstring>

#ifdef DRACO_TRANSCODER_SUPPORTED
#include <array>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "draco/attributes/geometry_attribute.h"
#include "draco/core/bounding_box.h"
#include "draco/core/draco_test_base.h"
#include "draco/core/draco_test_utils.h"
#include "draco/core/draco_types.h"
#include "draco/io/file_reader_factory.h"
#include "draco/io/file_reader_interface.h"
#include "draco/io/file_utils.h"
#include "draco/io/gltf_decoder.h"
#include "draco/io/gltf_test_helper.h"
#include "draco/io/parser_utils.h"
#include "draco/io/texture_io.h"
#include "draco/material/material_utils.h"
#include "draco/mesh/mesh_utils.h"
#include "draco/scene/audio.h"
#include "draco/scene/mesh_group.h"
#include "draco/scene/scene.h"
#include "draco/scene/scene_indices.h"
#include "draco/scene/scene_utils.h"
#include "draco/texture/texture_utils.h"
#include "gtest/gtest.h"

#ifdef DRACO_PMC_SUPPORTED
#include "draco/compression/pmc/pmc_utils.h"
#endif  // DRACO_PMC_SUPPORTED

namespace draco {

namespace {
std::unique_ptr<Scene> DecodeFullPathGltfFileToScene(
    const std::string& file_name) {
  GltfDecoder decoder;

  auto maybe_scene = decoder.DecodeFromFileToScene(file_name);
  if (!maybe_scene.ok()) {
    std::cout << maybe_scene.status().error_msg_string() << std::endl;
    return nullptr;
  }
  std::unique_ptr<Scene> scene = std::move(maybe_scene).value();
  return scene;
}

std::unique_ptr<Scene> DecodeTestGltfFileToScene(const std::string& file_name) {
  const std::string path = GetTestFileFullPath(file_name);
  return DecodeFullPathGltfFileToScene(path);
}
}  // namespace

class GltfEncoderTest : public ::testing::Test {
 protected:
  // This function searches for the |search| string and checks that there are at
  // least |count| occurrences.
  void CheckGltfFileAtLeastStringCount(const std::string& gltf_file,
                                       const std::string& search, int count) {
    std::vector<char> data;
    ASSERT_TRUE(ReadFileToBuffer(gltf_file, &data));

    draco::DecoderBuffer buffer;
    buffer.Init(data.data(), data.size());

    int strings_found = 0;
    do {
      std::string gltf_line;
      draco::parser::ParseLine(&buffer, &gltf_line);
      if (gltf_line.empty()) {
        break;
      }

      if (gltf_line.find(search) != std::string::npos) {
        strings_found++;
      }
      // No need to keep counting pass |count|.
    } while (strings_found < count);
    ASSERT_GE(strings_found, count);
  }

  // This function searches for the |search| string and checks that there are no
  // occurrences.
  void CheckGltfFileNoString(const std::string& gltf_file,
                             const std::string& search) {
    std::vector<char> data;
    ASSERT_TRUE(ReadFileToBuffer(gltf_file, &data));

    draco::DecoderBuffer buffer;
    buffer.Init(data.data(), data.size());

    do {
      std::string gltf_line;
      draco::parser::ParseLine(&buffer, &gltf_line);
      if (gltf_line.empty()) {
        break;
      }
      ASSERT_TRUE(gltf_line.find(search) == std::string::npos);
    } while (true);
  }

  void CheckAnimationAccessors(const Scene& scene,
                               int expected_num_input_accessors,
                               int expected_num_output_accessors) {
    int num_input_accessors = 0;
    int num_output_accessors = 0;

    for (int i = 0; i < scene.NumAnimations(); ++i) {
      const Animation* anim = scene.GetAnimation(AnimationIndex(i));
      ASSERT_NE(anim, nullptr);

      // The animation accessors in Draco are relative to the Animation object.
      // While in glTF the animation accessors are relative to the global
      // accessors.
      std::unordered_set<int> seen_accessors;

      for (int j = 0; j < anim->NumSamplers(); ++j) {
        const AnimationSampler* const sampler = anim->GetSampler(j);
        ASSERT_NE(sampler, nullptr);

        if (seen_accessors.find(sampler->input_index) == seen_accessors.end()) {
          seen_accessors.insert(sampler->input_index);
          num_input_accessors++;
        }
        if (seen_accessors.find(sampler->output_index) ==
            seen_accessors.end()) {
          seen_accessors.insert(sampler->output_index);
          num_output_accessors++;
        }
      }
    }

    EXPECT_EQ(expected_num_input_accessors, num_input_accessors);
    EXPECT_EQ(expected_num_output_accessors, num_output_accessors);
  }

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

  void CompareMeshes(const Mesh* mesh0, const Mesh* mesh1) {
    ASSERT_EQ(mesh0->num_faces(), mesh1->num_faces());
    ASSERT_EQ(mesh0->num_attributes(), mesh1->num_attributes());
    for (int att_id = 0; att_id < mesh0->num_attributes(); ++att_id) {
      const GeometryAttribute::Type att_type =
          mesh0->attribute(att_id)->attribute_type();
      const PointAttribute* const att = mesh1->GetNamedAttribute(att_type);
      ASSERT_EQ(mesh0->attribute(att_id)->size(), att->size())
          << "Attribute id:" << att_id
          << " with type:" << GeometryAttribute::TypeToString(att_type)
          << " is not equal.";
    }

    // Check materials are the same.
    if (mesh0->GetMaterialLibrary().NumMaterials() == 0) {
      // We add a default material if the source had no materials.
      ASSERT_EQ(mesh1->GetMaterialLibrary().NumMaterials(), 1);
    } else if (mesh1->GetMaterialLibrary().NumMaterials() == 0) {
      // We add a default material if the source had no materials.
      ASSERT_EQ(mesh0->GetMaterialLibrary().NumMaterials(), 1);
    } else {
      ASSERT_EQ(mesh0->GetMaterialLibrary().NumMaterials(),
                mesh1->GetMaterialLibrary().NumMaterials());
      for (int i = 0; i < mesh0->GetMaterialLibrary().NumMaterials(); ++i) {
        ASSERT_EQ(mesh0->GetMaterialLibrary().GetMaterial(i)->NumTextureMaps(),
                  mesh1->GetMaterialLibrary().GetMaterial(i)->NumTextureMaps());
        ASSERT_EQ(mesh0->GetMaterialLibrary().GetMaterial(i)->GetName(),
                  mesh1->GetMaterialLibrary().GetMaterial(i)->GetName());
      }
    }

    // Compare position bounding boxes.
    const PointAttribute* pos0 =
        mesh0->GetNamedAttribute(GeometryAttribute::POSITION);
    const PointAttribute* pos1 =
        mesh1->GetNamedAttribute(GeometryAttribute::POSITION);
    if (pos0 && pos1) {
      const BoundingBox bbox0 = mesh0->ComputeBoundingBox();
      const BoundingBox bbox1 = mesh1->ComputeBoundingBox();
      Compare(bbox0, bbox1, 0.01f * bbox0.Size().MinCoeff(), "Position");
    }

#ifdef DRACO_PMC_SUPPORTED
    // Compare texture coordinate bounding boxes.
    const PointAttribute* tc0 =
        mesh0->GetNamedAttribute(GeometryAttribute::TEX_COORD);
    const PointAttribute* tc1 =
        mesh1->GetNamedAttribute(GeometryAttribute::TEX_COORD);
    if (tc0 && tc1) {
      const auto bbox0 = PmcUtils::ComputeBoundingBox<float, 2>(*tc0);
      const auto bbox1 = PmcUtils::ComputeBoundingBox<float, 2>(*tc1);
      Compare(bbox0, bbox1, 0.01f * bbox0.Size().MinCoeff(), "TexCoord");
    }

    // Compare normal bounding boxes.
    const PointAttribute* n0 =
        mesh0->GetNamedAttribute(GeometryAttribute::NORMAL);
    const PointAttribute* n1 =
        mesh1->GetNamedAttribute(GeometryAttribute::NORMAL);
    if (n0 && n1) {
      BoundingBox bbox0 = PmcUtils::ComputeBoundingBox<float, 3>(*n0);
      BoundingBox bbox1 = PmcUtils::ComputeBoundingBox<float, 3>(*n1);
      Compare(bbox0, bbox1, 0.01f * bbox0.Size().MinCoeff(), "Normal");
    }

    // Compare color bounding boxes.
    const PointAttribute* c0 =
        mesh0->GetNamedAttribute(GeometryAttribute::COLOR);
    const PointAttribute* c1 =
        mesh1->GetNamedAttribute(GeometryAttribute::COLOR);
    if (c0 && c1) {
      ASSERT_EQ(c0->num_components(), 4);
      ASSERT_EQ(c1->num_components(), 4);
      const auto bbox0 = PmcUtils::ComputeBoundingBox<int32_t, 4>(*c0);
      const auto bbox1 = PmcUtils::ComputeBoundingBox<int32_t, 4>(*c1);
      Compare(bbox0, bbox1, 0, "Color");
    }

    // Compare weights bounding boxes.
    const PointAttribute* w0 =
        mesh0->GetNamedAttribute(GeometryAttribute::WEIGHTS);
    const PointAttribute* w1 =
        mesh1->GetNamedAttribute(GeometryAttribute::WEIGHTS);
    if (w0 && w1) {
      ASSERT_EQ(w0->num_components(), 4);
      ASSERT_EQ(w1->num_components(), 4);
      const auto bbox0 = PmcUtils::ComputeBoundingBox<float, 4>(*w0);
      const auto bbox1 = PmcUtils::ComputeBoundingBox<float, 4>(*w1);
      Compare(bbox0, bbox1, 0.01f * bbox0.Size().MinCoeff(), "Weights");
    }

    // Compare joints bounding boxes.
    const PointAttribute* j0 =
        mesh0->GetNamedAttribute(GeometryAttribute::JOINTS);
    const PointAttribute* j1 =
        mesh1->GetNamedAttribute(GeometryAttribute::JOINTS);
    if (j0 && j1) {
      ASSERT_EQ(j0->num_components(), 4);
      ASSERT_EQ(j1->num_components(), 4);
      const auto bbox0 = PmcUtils::ComputeBoundingBox<int32_t, 4>(*j0);
      const auto bbox1 = PmcUtils::ComputeBoundingBox<int32_t, 4>(*j1);
      Compare(bbox0, bbox1, 0, "Joints");
    }
#endif  // DRACO_PMC_SUPPORTED
  }

  void CompareScenes(const Scene* scene0, const Scene* scene1) {
    ASSERT_EQ(scene0->NumMeshes(), scene1->NumMeshes());
    ASSERT_EQ(scene0->NumMeshGroups(), scene1->NumMeshGroups());
    ASSERT_EQ(scene0->NumNodes(), scene1->NumNodes());
    ASSERT_EQ(scene0->GetMaterialLibrary().NumMaterials(),
              scene1->GetMaterialLibrary().NumMaterials());
    ASSERT_EQ(scene0->NumAnimations(), scene1->NumAnimations());
    ASSERT_EQ(scene0->NumSkins(), scene1->NumSkins());
    ASSERT_EQ(scene0->NumLights(), scene1->NumLights());

    // Check materials are the same.
    for (int i = 0; i < scene0->GetMaterialLibrary().NumMaterials(); ++i) {
      ASSERT_EQ(scene0->GetMaterialLibrary().GetMaterial(i)->NumTextureMaps(),
                scene1->GetMaterialLibrary().GetMaterial(i)->NumTextureMaps());
      ASSERT_EQ(scene0->GetMaterialLibrary().GetMaterial(i)->GetName(),
                scene1->GetMaterialLibrary().GetMaterial(i)->GetName());
    }

    // Check that materials variants names are the same.
    ASSERT_EQ(scene0->GetMaterialLibrary().NumMaterialsVariants(),
              scene1->GetMaterialLibrary().NumMaterialsVariants());
    for (int i = 0; i < scene0->GetMaterialLibrary().NumMaterialsVariants();
         i++) {
      ASSERT_EQ(scene0->GetMaterialLibrary().GetMaterialsVariantName(i),
                scene1->GetMaterialLibrary().GetMaterialsVariantName(i));
    }

    // Check that Cesium RTC values are the same.
    ASSERT_EQ(scene0->GetCesiumRtc(), scene1->GetCesiumRtc());

    // Check Nodes are the same.
    for (draco::SceneNodeIndex i(0); i < scene0->NumNodes(); ++i) {
      const SceneNode* const scene_node0 = scene0->GetNode(i);
      const SceneNode* const scene_node1 = scene1->GetNode(i);
      ASSERT_NE(scene_node0, nullptr);
      ASSERT_NE(scene_node1, nullptr);
      ASSERT_EQ(scene_node0->GetName(), scene_node1->GetName());
      ASSERT_EQ(scene_node0->GetLightIndex(), scene_node1->GetLightIndex());
    }

    // Check MeshGroups are the same.
    for (draco::MeshGroupIndex i(0); i < scene0->NumMeshGroups(); ++i) {
      const MeshGroup* const mesh_group0 = scene0->GetMeshGroup(i);
      const MeshGroup* const mesh_group1 = scene1->GetMeshGroup(i);
      ASSERT_NE(mesh_group0, nullptr);
      ASSERT_NE(mesh_group1, nullptr);
      ASSERT_EQ(mesh_group0->GetName(), mesh_group1->GetName());
      ASSERT_EQ(mesh_group0->NumMeshInstances(),
                mesh_group1->NumMeshInstances());

      // Check that mesh instances are the same.
      for (int j = 0; j < mesh_group1->NumMeshInstances(); j++) {
        const MeshGroup::MeshInstance& instance0 =
            mesh_group0->GetMeshInstance(j);
        const MeshGroup::MeshInstance& instance1 =
            mesh_group1->GetMeshInstance(j);
        ASSERT_EQ(instance0.mesh_index, instance1.mesh_index);
        ASSERT_EQ(instance0.material_index, instance1.material_index);
        ASSERT_EQ(instance0.materials_variants_mappings.size(),
                  instance1.materials_variants_mappings.size());

        // Check that materials variants mappings are the same.
        for (int k = 0; k < instance0.materials_variants_mappings.size(); k++) {
          const MeshGroup::MaterialsVariantsMapping& mapping0 =
              instance0.materials_variants_mappings[k];
          const MeshGroup::MaterialsVariantsMapping& mapping1 =
              instance1.materials_variants_mappings[k];
          ASSERT_EQ(mapping0.material, mapping1.material);
          ASSERT_EQ(mapping0.variants.size(), mapping1.variants.size());
          for (int l = 0; l < mapping0.variants.size(); l++) {
            ASSERT_EQ(mapping0.variants[l], mapping1.variants[l]);
          }
        }
      }
    }

    // Check Animations are the same.
    for (draco::AnimationIndex i(0); i < scene0->NumAnimations(); ++i) {
      const Animation* const animation0 = scene0->GetAnimation(i);
      const Animation* const animation1 = scene1->GetAnimation(i);
      ASSERT_NE(animation0, nullptr);
      ASSERT_NE(animation1, nullptr);
      ASSERT_EQ(animation0->NumSamplers(), animation1->NumSamplers());
      ASSERT_EQ(animation0->NumChannels(), animation1->NumChannels());
      ASSERT_EQ(animation0->NumNodeAnimationData(),
                animation1->NumNodeAnimationData());
    }

    // Check that lights are the same.
    for (draco::LightIndex i(0); i < scene0->NumLights(); ++i) {
      const Light* const light0 = scene0->GetLight(i);
      const Light* const light1 = scene1->GetLight(i);
      ASSERT_NE(light0, nullptr);
      ASSERT_NE(light1, nullptr);
      ASSERT_EQ(light0->GetName(), light1->GetName());
      ASSERT_EQ(light0->GetColor(), light1->GetColor());
      ASSERT_EQ(light0->GetIntensity(), light1->GetIntensity());
      ASSERT_EQ(light0->GetType(), light1->GetType());
      ASSERT_EQ(light0->GetRange(), light1->GetRange());
      if (light0->GetType() == Light::SPOT) {
        ASSERT_EQ(light0->GetInnerConeAngle(), light1->GetInnerConeAngle());
        ASSERT_EQ(light0->GetOuterConeAngle(), light1->GetOuterConeAngle());
      }
    }

    // Check that pass-through extensions are the same.
    ASSERT_EQ(scene0->GetPassThroughExtensions(),
              scene1->GetPassThroughExtensions());
  }

  void EncodeMeshToFile(const Mesh& mesh,
                        const std::string& gltf_file_full_path) {
    std::string folder_path;
    std::string gltf_file_name;
    draco::SplitPath(gltf_file_full_path, &folder_path, &gltf_file_name);
    GltfEncoder gltf_encoder;
    ASSERT_TRUE(
        gltf_encoder.EncodeToFile<Mesh>(mesh, gltf_file_full_path, folder_path))
        << "Failed gltf_file_full_path:" << gltf_file_full_path
        << " folder_path:" << folder_path;
  }

  void EncodeSceneToFile(const Scene& scene,
                         const std::string& gltf_file_full_path) {
    std::string folder_path;
    std::string gltf_file_name;
    draco::SplitPath(gltf_file_full_path, &folder_path, &gltf_file_name);
    GltfEncoder gltf_encoder;
    ASSERT_TRUE(gltf_encoder.EncodeToFile<Scene>(scene, gltf_file_full_path,
                                                 folder_path))
        << "Failed gltf_file_full_path:" << gltf_file_full_path
        << " folder_path:" << folder_path;
  }

  // Encode |mesh| to a temporary glTF file. Then decode the glTF file and
  // return the mesh in |mesh_gltf|.
  void MeshToDecodedGltfMesh(const Mesh& mesh,
                             std::unique_ptr<Mesh>* mesh_gltf) {
    const std::string gltf_file_full_path =
        draco::GetTestTempFileFullPath("test.gltf");
    EncodeMeshToFile(mesh, gltf_file_full_path);
    *mesh_gltf = std::move(ReadMeshFromFile(gltf_file_full_path)).value();
    ASSERT_NE(*mesh_gltf, nullptr);
  }

  // Encode |mesh| to a temporary glTF file. Then decode the glTF file as a
  // scene and return it in |scene_gltf|.
  void MeshToDecodedGltfScene(const Mesh& mesh,
                              std::unique_ptr<Scene>* scene_gltf) {
    const std::string gltf_file_full_path =
        draco::GetTestTempFileFullPath("test.gltf");
    EncodeMeshToFile(mesh, gltf_file_full_path);
    *scene_gltf = std::move(ReadSceneFromFile(gltf_file_full_path)).value();
    ASSERT_NE(*scene_gltf, nullptr);
  }

  // Encode |scene| to a temporary glTF file. Then decode the glTF file and
  // return the scene in |scene_gltf|.
  void SceneToDecodedGltfScene(const Scene& scene,
                               const std::string& temp_basename,
                               std::unique_ptr<Scene>* scene_gltf) {
    const std::string gltf_file_full_path =
        draco::GetTestTempFileFullPath(temp_basename);
    EncodeSceneToFile(scene, gltf_file_full_path);

    *scene_gltf = DecodeFullPathGltfFileToScene(gltf_file_full_path);
    if (SceneUtils::IsDracoCompressionEnabled(scene) &&
        !AreAllCompressedMeshesDegenerate(scene)) {
      // Two occurrences of a mesh compression extension string is the least
      // amount for a valid compressed glTF file.
#ifdef DRACO_PMC_SUPPORTED
      const std::string khr_mesh_compression =
          scene.GetMesh(MeshIndex(0)).GetCompressionOptions().use_pmc
              ? "KHR_pmc_mesh_compression"
              : "KHR_draco_mesh_compression";
#else
      const std::string khr_mesh_compression = "KHR_draco_mesh_compression";
#endif  // DRACO_PMC_SUPPORTED
      CheckGltfFileAtLeastStringCount(gltf_file_full_path, khr_mesh_compression,
                                      2);
    }
    ASSERT_NE((*scene_gltf).get(), nullptr);
  }

  void SceneToDecodedGltfScene(const Scene& scene,
                               std::unique_ptr<Scene>* scene_gltf) {
    SceneToDecodedGltfScene(scene, "test.gltf", scene_gltf);
  }

  bool AreAllCompressedMeshesDegenerate(const Scene& scene) const {
    for (MeshIndex mi(0); mi < scene.NumMeshes(); ++mi) {
      const auto& mesh = scene.GetMesh(mi);
      if (mesh.IsCompressionEnabled() &&
          !MeshUtils::AreAllFacePositionsDegenerate(mesh)) {
        return false;
      }
    }
    return true;
  }

  void EncodeMeshToGltfAndCompare(Mesh* mesh) {
    ASSERT_GT(mesh->num_faces(), 0);

    std::unique_ptr<Mesh> mesh_from_gltf;
    MeshToDecodedGltfMesh(*mesh, &mesh_from_gltf);

    mesh->DeduplicatePointIds();
    ASSERT_TRUE(mesh->DeduplicateAttributeValues());
    CompareMeshes(mesh, mesh_from_gltf.get());
  }

  void EncodeSceneToGltfAndCompare(Scene* scene) {
    std::unique_ptr<Scene> scene_from_gltf;
    SceneToDecodedGltfScene(*scene, &scene_from_gltf);
    if (!SceneUtils::IsDracoCompressionEnabled(*scene)) {
      CompareScenes(scene, scene_from_gltf.get());
    }
  }

  void EnableCompression(Mesh* mesh, bool use_pmc) {
    DracoCompressionOptions options;
#ifdef DRACO_PMC_SUPPORTED
    options.use_pmc = use_pmc;
#endif  // DRACO_PMC_SUPPORTED
    options.quantization_position.SetQuantizationBits(12);
    mesh->SetCompressionEnabled(true);
    mesh->SetCompressionOptions(options);
  }

  void test_encoding(const std::string& file_name, bool compress = false,
                     bool use_pmc = false) {
    const std::unique_ptr<Mesh> mesh(ReadMeshFromTestFile(file_name, true));

    ASSERT_NE(mesh, nullptr) << "Failed to load test model " << file_name;

    // Enable Draco or PMC compression.
    if (compress) {
      EnableCompression(mesh.get(), use_pmc);
    }

    if (!use_pmc) {
      // Draco compression or no compression.
      EncodeMeshToGltfAndCompare(mesh.get());
      return;
    }

#ifdef DRACO_PMC_SUPPORTED
    // Encode and decode mesh to glTF with PMC.
    std::unique_ptr<Mesh> mesh_from_gltf_pmc;
    MeshToDecodedGltfMesh(*mesh, &mesh_from_gltf_pmc);
    mesh_from_gltf_pmc->DeduplicatePointIds();
    ASSERT_TRUE(mesh_from_gltf_pmc->DeduplicateAttributeValues());

    // Encode and decode mesh to glTF with Draco.
    Mesh mesh_draco;
    mesh_draco.Copy(*mesh);
    mesh_draco.GetCompressionOptions().use_pmc = false;
    std::unique_ptr<Mesh> mesh_from_gltf_draco;
    MeshToDecodedGltfMesh(mesh_draco, &mesh_from_gltf_draco);
    mesh_from_gltf_draco->DeduplicatePointIds();

    // Compare Draco and PMC meshes. Quantization may change the number of
    // values and points, so for larger meshes we cannot easily compare the
    // encoded / decoded mesh with the original mesh.
    CompareMeshes(mesh_from_gltf_draco.get(), mesh_from_gltf_pmc.get());
#endif  // DRACO_PMC_SUPPORTED
  }
};

TEST_F(GltfEncoderTest, AnimationPointerEncoding) {
  const std::string file_name = "sphere_animation_pointer.gltf";
  auto scene = DecodeTestGltfFileToScene(file_name);
  ASSERT_NE(scene, nullptr);

  const std::string gltf_file_full_path =
      draco::GetTestTempFileFullPath("test_pointer.gltf");
  EncodeSceneToFile(*scene, gltf_file_full_path);

  // Check that KHR_animation_pointer is in the output file.
  CheckGltfFileAtLeastStringCount(gltf_file_full_path, "KHR_animation_pointer",
                                  4);

  // Check that the pointers are preserved.
  CheckGltfFileAtLeastStringCount(
      gltf_file_full_path, "/materials/0/pbrMetallicRoughness/baseColorFactor",
      1);
  CheckGltfFileAtLeastStringCount(
      gltf_file_full_path,
      "/materials/0/pbrMetallicRoughness/baseColorTexture/extensions/"
      "KHR_texture_transform/offset",
      1);
  CheckGltfFileAtLeastStringCount(
      gltf_file_full_path,
      "/materials/0/extensions/KHR_materials_emissive_strength/"
      "emissiveStrength",
      1);
  CheckGltfFileAtLeastStringCount(gltf_file_full_path, "/nodes/0/translation",
                                  1);
}

TEST_F(GltfEncoderTest, TestGltfEncodingAll) {
  // Test decoded mesh from encoded glTF file stays the same.
  test_encoding("test_nm.obj.edgebreaker.cl4.2.2.drc");
  test_encoding("cube_att.drc");
  test_encoding("car.drc");
  test_encoding("bunny_gltf.drc");
}

TEST_F(GltfEncoderTest, TestGltfEncodingWithDracoCompression) {
  constexpr bool kCompress = true;
  constexpr bool kUsePmc = false;
  test_encoding("cube_att.obj", kCompress, kUsePmc);
  test_encoding("two_cubes_att.obj", kCompress, kUsePmc);
  test_encoding("sphere_mr.obj", kCompress, kUsePmc);
}

#ifdef DRACO_PMC_SUPPORTED
// Tests a mesh with color attribute without alpha component (RGB).
TEST_F(GltfEncoderTest, SceneColorAttributeNoAlphaGltf) {
  // Read mesh from PLY file.
  const std::string file_name = "test_pos_color_no_alpha_ascii.ply";
  auto mesh = draco::ReadMeshFromTestFile(file_name);
  ASSERT_NE(mesh, nullptr);

  // Check the color attribute info.
  const auto* color_att = mesh->GetNamedAttribute(GeometryAttribute::COLOR);
  ASSERT_NE(color_att, nullptr);
  ASSERT_EQ(color_att->num_components(), 3);
  ASSERT_EQ(color_att->data_type(), DataType::DT_UINT8);

  // Wrap the mesh in a scene and enable PMC compression.
  auto maybe_scene = SceneUtils::MeshToScene(std::move(mesh));
  ASSERT_TRUE(maybe_scene.ok());
  auto scene = std::move(maybe_scene).value();
  constexpr bool kUsePmc = true;
  EnableCompression(&scene->GetMesh(MeshIndex(0)), kUsePmc);

  // Encode the scene to glTF and decode it back to a scene.
  std::unique_ptr<Scene> decoded_scene;
  SceneToDecodedGltfScene(*scene, "test_no_alpha.gltf", &decoded_scene);

  // Check the decoded scene.
  ASSERT_EQ(decoded_scene->NumMeshes(), 1);
  const auto* decoded_mesh = &decoded_scene->GetMesh(MeshIndex(0));
  ASSERT_EQ(decoded_mesh->num_attributes(), 2);
  const auto* decoded_color_att =
      decoded_mesh->GetNamedAttribute(GeometryAttribute::COLOR);
  ASSERT_NE(decoded_color_att, nullptr);
  ASSERT_EQ(decoded_color_att->num_components(), 3);
  ASSERT_EQ(decoded_color_att->data_type(), DataType::DT_UINT8);
  CompareScenes(scene.get(), decoded_scene.get());
}

// Tests a mesh with float color attribute.
TEST_F(GltfEncoderTest, SimpleColorAttributeFloatGltf) {
  auto mesh = std::make_unique<Mesh>();
  // Add 3 positions.
  GeometryAttribute pos_att;
  pos_att.Init(GeometryAttribute::POSITION, nullptr, 3, DT_FLOAT32, false,
               3 * sizeof(float), 0);
  const uint32_t pos_att_id = mesh->AddAttribute(pos_att, true, 3);
  mesh->attribute(pos_att_id)
      ->SetAttributeValue(draco::AttributeValueIndex(0),
                          draco::Vector3f(0, 0, 0).data());
  mesh->attribute(pos_att_id)
      ->SetAttributeValue(draco::AttributeValueIndex(1),
                          draco::Vector3f(1, 0, 0).data());
  mesh->attribute(pos_att_id)
      ->SetAttributeValue(draco::AttributeValueIndex(2),
                          draco::Vector3f(0, 1, 0).data());

  // Add 3 varying colors.
  GeometryAttribute col_att;
  col_att.Init(GeometryAttribute::COLOR, nullptr, 4, DT_FLOAT32, false,
               4 * sizeof(float), 0);
  const uint32_t col_att_id = mesh->AddAttribute(col_att, true, 3);
  mesh->attribute(col_att_id)
      ->SetAttributeValue(draco::AttributeValueIndex(0),
                          draco::Vector4f(1, 0, 0, 1).data());
  mesh->attribute(col_att_id)
      ->SetAttributeValue(draco::AttributeValueIndex(1),
                          draco::Vector4f(0, 1, 0, 0).data());
  mesh->attribute(col_att_id)
      ->SetAttributeValue(draco::AttributeValueIndex(2),
                          draco::Vector4f(0, 0, 1, 1).data());

  mesh->SetNumFaces(1);
  draco::Mesh::Face face;
  face[0] = draco::PointIndex(0);
  face[1] = draco::PointIndex(1);
  face[2] = draco::PointIndex(2);
  mesh->SetFace(draco::FaceIndex(0), face);

  // Wrap the mesh in a scene and enable PMC compression.
  constexpr bool kUsePmc = true;
  EnableCompression(mesh.get(), kUsePmc);
  auto maybe_scene = SceneUtils::MeshToScene(std::move(mesh));
  ASSERT_TRUE(maybe_scene.ok());
  auto scene = std::move(maybe_scene).value();

  // Encode the scene to glTF and decode it back to a scene.
  std::unique_ptr<Scene> decoded_scene;
  SceneToDecodedGltfScene(*scene, &decoded_scene);

  // Check the decoded scene.
  ASSERT_EQ(decoded_scene->NumMeshes(), 1);
  const auto* decoded_mesh = &decoded_scene->GetMesh(MeshIndex(0));
  ASSERT_EQ(decoded_mesh->num_attributes(), 2);
  const auto* decoded_color_att =
      decoded_mesh->GetNamedAttribute(GeometryAttribute::COLOR);
  ASSERT_NE(decoded_color_att, nullptr);
  ASSERT_EQ(decoded_color_att->num_components(), 4);
  ASSERT_EQ(decoded_color_att->data_type(), DataType::DT_FLOAT32);
  CompareScenes(scene.get(), decoded_scene.get());
}

// Tests a mesh with color attribute.
TEST_F(GltfEncoderTest, SceneColorAttributeGltf) {
  // Read scene from glTF file.
  const std::string file_name = "test_pos_color.gltf";
  auto scene = draco::ReadSceneFromTestFile(file_name);
  ASSERT_NE(scene, nullptr);
  ASSERT_EQ(scene->NumMeshes(), 1);

  // Get a mesh from the scene and enable PMC compression.
  auto& mesh = scene->GetMesh(MeshIndex(0));
  ASSERT_EQ(mesh.num_attributes(), 2);
  ASSERT_EQ(mesh.NumNamedAttributes(GeometryAttribute::POSITION), 1);
  ASSERT_EQ(mesh.NumNamedAttributes(GeometryAttribute::COLOR), 1);
  constexpr bool kUsePmc = true;
  EnableCompression(&mesh, kUsePmc);

  // Encode the scene to glTF and decode it back to a scene.
  std::unique_ptr<Scene> decoded_scene;
  SceneToDecodedGltfScene(*scene, &decoded_scene);

  // Check the decoded scene.
  ASSERT_EQ(decoded_scene->NumMeshes(), 1);
  auto decoded_mesh = &decoded_scene->GetMesh(MeshIndex(0));
  ASSERT_EQ(decoded_mesh->num_attributes(), 2);
  ASSERT_EQ(decoded_mesh->NumNamedAttributes(GeometryAttribute::POSITION), 1);
  ASSERT_EQ(decoded_mesh->NumNamedAttributes(GeometryAttribute::COLOR), 1);
  CompareScenes(scene.get(), decoded_scene.get());
}

// Tests a mesh with normal attribute and octahedral quantization.
TEST_F(GltfEncoderTest, SceneNormalAttributeOctahedralGltf) {
  // Read scene from glTF file.
  const std::string file_name = "sphere.gltf";
  auto scene = draco::ReadSceneFromTestFile(file_name);
  ASSERT_NE(scene, nullptr);
  ASSERT_EQ(scene->NumMeshes(), 1);

  // Get a mesh from the scene and enable PMC compression.
  auto& mesh = scene->GetMesh(MeshIndex(0));
  ASSERT_GT(mesh.num_attributes(), 0);
  ASSERT_EQ(mesh.NumNamedAttributes(GeometryAttribute::POSITION), 1);
  ASSERT_EQ(mesh.NumNamedAttributes(GeometryAttribute::NORMAL), 1);
  constexpr bool kUsePmc = true;
  EnableCompression(&mesh, kUsePmc);

  // Encode the scene to glTF and decode it back to a scene.
  std::unique_ptr<Scene> decoded_scene;
  SceneToDecodedGltfScene(*scene, &decoded_scene);

  // Check the decoded scene.
  ASSERT_EQ(decoded_scene->NumMeshes(), 1);
  CompareScenes(scene.get(), decoded_scene.get());
  auto decoded_mesh = &decoded_scene->GetMesh(MeshIndex(0));
  ASSERT_EQ(decoded_mesh->NumNamedAttributes(GeometryAttribute::POSITION), 1);
  ASSERT_EQ(decoded_mesh->NumNamedAttributes(GeometryAttribute::NORMAL), 1);

  // Compare normal bounding boxes to verify that reconstructed normals are
  // within the expected range.
  const auto* n0 = mesh.GetNamedAttribute(GeometryAttribute::NORMAL);
  const auto* n1 = decoded_mesh->GetNamedAttribute(GeometryAttribute::NORMAL);
  ASSERT_EQ(n0->size(), n1->size());
  const BoundingBox bbox0 = PmcUtils::ComputeBoundingBox<float, 3>(*n0);
  const BoundingBox bbox1 = PmcUtils::ComputeBoundingBox<float, 3>(*n1);
  Compare(bbox0, bbox1, 0.01f * bbox0.Size().MinCoeff(), "Normal");
}

// Tests a mesh with joints and weights attributes.
TEST_F(GltfEncoderTest, SceneJointsAndWeightsAttributesGltf) {
  // Read scene from glTF file.
  const std::string file_name = "CesiumMan/glTF/CesiumMan.gltf";
  auto scene = draco::ReadSceneFromTestFile(file_name);
  ASSERT_NE(scene, nullptr);
  ASSERT_EQ(scene->NumMeshes(), 1);

  // Get a mesh from the scene.
  auto mesh = &scene->GetMesh(MeshIndex(0));
  ASSERT_EQ(mesh->num_attributes(), 5);
  ASSERT_EQ(mesh->NumNamedAttributes(GeometryAttribute::JOINTS), 1);
  ASSERT_EQ(mesh->NumNamedAttributes(GeometryAttribute::WEIGHTS), 1);

  // Check JOINTS attribute info.
  ASSERT_EQ(mesh->attribute(0)->attribute_type(), GeometryAttribute::JOINTS);
  ASSERT_EQ(mesh->attribute(0)->num_components(), 4);
  ASSERT_EQ(mesh->attribute(0)->data_type(), DT_UINT16);

  // Check WEIGHTS attribute info.
  ASSERT_EQ(mesh->attribute(4)->attribute_type(), GeometryAttribute::WEIGHTS);
  ASSERT_EQ(mesh->attribute(4)->num_components(), 4);
  ASSERT_EQ(mesh->attribute(4)->data_type(), DT_FLOAT32);

  // Enable PMC compression.
  constexpr bool kUsePmc = true;
  EnableCompression(mesh, kUsePmc);

  // Encode the scene to glTF and decode it back to a scene.
  std::unique_ptr<Scene> scene_gltf_pmc;
  SceneToDecodedGltfScene(*scene, &scene_gltf_pmc);

  // Check the decoded scene.
  ASSERT_EQ(scene_gltf_pmc->NumMeshes(), 1);
  auto mesh_gltf_pmc = &scene_gltf_pmc->GetMesh(MeshIndex(0));
  ASSERT_EQ(mesh_gltf_pmc->num_attributes(), 5);
  ASSERT_EQ(mesh->NumNamedAttributes(GeometryAttribute::POSITION), 1);
  ASSERT_EQ(mesh->NumNamedAttributes(GeometryAttribute::WEIGHTS), 1);
  ASSERT_EQ(mesh->NumNamedAttributes(GeometryAttribute::JOINTS), 1);
  CompareScenes(scene.get(), scene_gltf_pmc.get());
}
#endif  // DRACO_PMC_SUPPORTED

TEST_F(GltfEncoderTest, ImportTangentAttribute) {
  auto mesh = draco::ReadMeshFromTestFile("sphere.gltf");
  ASSERT_NE(mesh, nullptr);

  const draco::PointAttribute* const tangent_att =
      mesh->GetNamedAttribute(draco::GeometryAttribute::TANGENT);
  ASSERT_NE(tangent_att, nullptr);

  std::unique_ptr<Mesh> mesh_from_gltf;
  MeshToDecodedGltfMesh(*mesh, &mesh_from_gltf);
  ASSERT_EQ(mesh->num_attributes(), mesh_from_gltf->num_attributes());
}

TEST_F(GltfEncoderTest, EncodeColorTexture) {
  const std::string tex_file_name = draco::GetTestFileFullPath("test.png");
  std::unique_ptr<draco::Texture> texture =
      draco::ReadTextureFromFile(tex_file_name).value();
  ASSERT_NE(texture, nullptr);

  std::unique_ptr<draco::Mesh> mesh =
      draco::ReadMeshFromTestFile("cube_att.obj");
  ASSERT_NE(mesh, nullptr);

  mesh->GetMaterialLibrary().MutableMaterial(0)->SetTextureMap(
      std::move(texture), draco::TextureMap::COLOR, 0);

  EncodeMeshToGltfAndCompare(mesh.get());
}

TEST_F(GltfEncoderTest, EncodeEmissiveStrength) {
  std::unique_ptr<draco::Mesh> mesh =
      draco::ReadMeshFromTestFile("cube_att.obj");
  ASSERT_NE(mesh, nullptr);

  Material* material = mesh->GetMaterialLibrary().MutableMaterial(0);
  material->SetHasEmissiveStrength(true);
  material->SetEmissiveStrength(5.0f);

  std::unique_ptr<Mesh> decoded_mesh;
  MeshToDecodedGltfMesh(*mesh, &decoded_mesh);

  ASSERT_GT(decoded_mesh->GetMaterialLibrary().NumMaterials(), 0);
  const Material* decoded_material =
      decoded_mesh->GetMaterialLibrary().GetMaterial(0);
  ASSERT_TRUE(decoded_material->HasEmissiveStrength());
  ASSERT_EQ(decoded_material->GetEmissiveStrength(), 5.0f);
}

TEST_F(GltfEncoderTest, EncodeColors) {
  auto mesh = draco::ReadMeshFromTestFile("test_pos_color.ply");
  ASSERT_NE(mesh, nullptr);

  const draco::PointAttribute* const color_att =
      mesh->GetNamedAttribute(draco::GeometryAttribute::COLOR);
  ASSERT_NE(color_att, nullptr);

  std::unique_ptr<Mesh> mesh_from_gltf;
  MeshToDecodedGltfMesh(*mesh, &mesh_from_gltf);

  ASSERT_EQ(mesh->num_faces(), mesh_from_gltf->num_faces());
  ASSERT_EQ(mesh->num_attributes(), mesh_from_gltf->num_attributes());
  ASSERT_EQ(
      mesh->NumNamedAttributes(draco::GeometryAttribute::COLOR),
      mesh_from_gltf->NumNamedAttributes(draco::GeometryAttribute::COLOR));
}

TEST_F(GltfEncoderTest, EncodeNamedGenericAttribute) {
  // Load some base mesh.
  auto mesh = draco::ReadMeshFromTestFile("test_generic.ply");
  ASSERT_NE(mesh, nullptr);
  const draco::PointAttribute* const pos_att =
      mesh->GetNamedAttribute(draco::GeometryAttribute::POSITION);
  ASSERT_NE(pos_att, nullptr);
  int num_vertices = pos_att->size();

  // Add two new scalar attributes where each value corresponds to the position
  // value index (vertex). The first attribute will have metadata, the second
  // attribute won't.
  std::unique_ptr<draco::PointAttribute> pa_0(new draco::PointAttribute());
  std::unique_ptr<draco::PointAttribute> pa_1(new draco::PointAttribute());
  pa_0->Init(draco::GeometryAttribute::GENERIC, /* scalar */ 1,
             draco::DT_FLOAT32, false,
             /* one value per position value */ num_vertices);
  pa_1->Init(draco::GeometryAttribute::GENERIC, /* scalar */ 1,
             draco::DT_FLOAT32, false,
             /* one value per position value */ num_vertices);

  // Set the values for the new attributes.
  for (draco::AttributeValueIndex avi(0); avi < num_vertices; ++avi) {
    const float att_value = avi.value();
    pa_0->SetAttributeValue(avi, &att_value);
    pa_1->SetAttributeValue(avi, &att_value);
  }

  // Add the attribute to the existing mesh.
  const int new_att_id_0 = mesh->AddPerVertexAttribute(std::move(pa_0));
  const int new_att_id_1 = mesh->AddPerVertexAttribute(std::move(pa_1));
  ASSERT_NE(new_att_id_0, -1);
  ASSERT_NE(new_att_id_1, -1);

  // Set metadata for first attribute so it gets written out by glTF encoder.
  std::unique_ptr<draco::AttributeMetadata> am(new draco::AttributeMetadata());
  constexpr char kAttributeName[] = "MyAttributeName";
  constexpr char kDracoMetadataGltfAttributeName[] =
      "//GLTF/ApplicationSpecificAttributeName";
  am->AddEntryString(kDracoMetadataGltfAttributeName, kAttributeName);
  mesh->AddAttributeMetadata(new_att_id_0, std::move(am));

  // Make sure the GLTF contains a reference to the named attribute.
  const std::string gltf_file_full_path =
      draco::GetTestTempFileFullPath("GenericAttribute.gltf");
  std::string folder_path;
  std::string gltf_file_name;
  draco::SplitPath(gltf_file_full_path, &folder_path, &gltf_file_name);
  GltfEncoder gltf_encoder;
  ASSERT_TRUE(gltf_encoder.EncodeToFile<Mesh>(*(mesh.get()),
                                              gltf_file_full_path, folder_path))
      << "Failed gltf_file_full_path:" << gltf_file_full_path
      << " folder_path:" << folder_path;
  CheckGltfFileAtLeastStringCount(gltf_file_full_path, kAttributeName, 1);

  // The decoder does not yet support generic attribute names, so instead of
  // using the decoder we compare against a golden file.
  const std::string gltf_generated_bin_filename =
      draco::GetTestTempFileFullPath("buffer0.bin");
  std::vector<char> generated_buffer;
  ASSERT_TRUE(ReadFileToBuffer(gltf_generated_bin_filename, &generated_buffer));
  std::string generated_str(generated_buffer.data(), generated_buffer.size());

  const std::string gltf_expected_bin_filename =
      GetTestFileFullPath("test_generic_golden.bin");
  const bool kUpdateGoldens = false;
  if (kUpdateGoldens) {
    ASSERT_TRUE(WriteBufferToFile(generated_buffer.data(),
                                  generated_buffer.size(),
                                  gltf_expected_bin_filename));
  }
  std::vector<char> expected_buffer;
  ASSERT_TRUE(ReadFileToBuffer(gltf_expected_bin_filename, &expected_buffer));
  std::string expected_str(expected_buffer.data(), expected_buffer.size());

  EXPECT_TRUE(generated_str == expected_str);
}

TEST_F(GltfEncoderTest, EncodeMetallicRoughnessTexture) {
  const std::string tex_file_name = draco::GetTestFileFullPath("test.png");
  std::unique_ptr<draco::Texture> texture =
      draco::ReadTextureFromFile(tex_file_name).value();
  ASSERT_NE(texture, nullptr);

  std::unique_ptr<draco::Mesh> mesh =
      draco::ReadMeshFromTestFile("cube_att.obj");
  ASSERT_NE(mesh, nullptr);

  mesh->GetMaterialLibrary().MutableMaterial(0)->SetTextureMap(
      std::move(texture), draco::TextureMap::METALLIC_ROUGHNESS, 0);

  EncodeMeshToGltfAndCompare(mesh.get());
}

TEST_F(GltfEncoderTest, EncodeOcclusionTexture) {
  const std::string tex_file_name = draco::GetTestFileFullPath("test.png");
  std::unique_ptr<draco::Texture> texture =
      draco::ReadTextureFromFile(tex_file_name).value();
  ASSERT_NE(texture, nullptr);

  std::unique_ptr<draco::Mesh> mesh =
      draco::ReadMeshFromTestFile("cube_att.obj");
  ASSERT_NE(mesh, nullptr);

  mesh->GetMaterialLibrary().MutableMaterial(0)->SetTextureMap(
      std::move(texture), draco::TextureMap::AMBIENT_OCCLUSION, 0);

  EncodeMeshToGltfAndCompare(mesh.get());
}

TEST_F(GltfEncoderTest, EncodeEmissiveTexture) {
  const std::string tex_file_name = draco::GetTestFileFullPath("test.png");
  std::unique_ptr<draco::Texture> texture =
      draco::ReadTextureFromFile(tex_file_name).value();
  ASSERT_NE(texture, nullptr);

  std::unique_ptr<draco::Mesh> mesh =
      draco::ReadMeshFromTestFile("cube_att.obj");
  ASSERT_NE(mesh, nullptr);

  mesh->GetMaterialLibrary().MutableMaterial(0)->SetTextureMap(
      std::move(texture), draco::TextureMap::EMISSIVE, 0);

  EncodeMeshToGltfAndCompare(mesh.get());
}

// Tests splitting the mesh into multiple primitives.
TEST_F(GltfEncoderTest, EncodeSplitMesh) {
  std::unique_ptr<draco::Mesh> mesh =
      draco::ReadMeshFromTestFile("CesiumMilkTruck/glTF/CesiumMilkTruck.gltf");
  ASSERT_NE(mesh, nullptr);
  const int32_t material_att_id =
      mesh->GetNamedAttributeId(draco::GeometryAttribute::MATERIAL);
  ASSERT_NE(material_att_id, -1);
  EncodeMeshToGltfAndCompare(mesh.get());
}

// Tests encoding a scene from a glTF with multiple meshes and primitives,
// including mesh instances.
TEST_F(GltfEncoderTest, EncodeInstancedScene) {
  const std::string file_name = "CesiumMilkTruck/glTF/CesiumMilkTruck.gltf";
  const std::unique_ptr<Scene> scene(DecodeTestGltfFileToScene(file_name));
  ASSERT_NE(scene, nullptr);

  std::unique_ptr<Scene> transcoded_scene;
  SceneToDecodedGltfScene(*scene, "EncodeInstancedScene.gltf",
                          &transcoded_scene);
  ASSERT_NE(transcoded_scene, nullptr);
  CompareScenes(scene.get(), transcoded_scene.get());
  EXPECT_EQ(transcoded_scene->NumAnimations(), 1);

  const int num_input_accessors = 2;
  const int num_output_accessors = 2;
  CheckAnimationAccessors(*transcoded_scene, num_input_accessors,
                          num_output_accessors);
}

// Tests encoding a scene from a glTF with multiple meshes and primitives,
// including mesh instances.
TEST_F(GltfEncoderTest, EncodeBoneAnimation) {
  const std::string file_name = "CesiumMan/glTF/CesiumMan.gltf";
  const std::unique_ptr<Scene> scene(DecodeTestGltfFileToScene(file_name));
  ASSERT_NE(scene, nullptr);

  std::unique_ptr<Scene> transcoded_scene;
  SceneToDecodedGltfScene(*scene, "EncodeBoneAnimation.gltf",
                          &transcoded_scene);
  ASSERT_NE(transcoded_scene, nullptr);
  CompareScenes(scene.get(), transcoded_scene.get());
  EXPECT_EQ(transcoded_scene->NumAnimations(), 1);

  const Animation* anim = scene->GetAnimation(AnimationIndex(0));
  ASSERT_NE(anim, nullptr);
  ASSERT_TRUE(anim->GetName().empty());

  // TODO(b/145703399): Figure out how to test that all of the input accessors
  // in animation channels in the encoded glTF file will be the same for this
  // test file.
  const int num_input_accessors = 57;
  const int num_output_accessors = 57;
  CheckAnimationAccessors(*transcoded_scene, num_input_accessors,
                          num_output_accessors);
}

// Tests encoding a scene from a glTF with nodes that have names.
TEST_F(GltfEncoderTest, EncodeSceneWithNodeNames) {
  const std::string file_name = "Lantern/glTF/Lantern.gltf";
  const std::unique_ptr<Scene> scene(DecodeTestGltfFileToScene(file_name));
  ASSERT_NE(scene, nullptr);
  EncodeSceneToGltfAndCompare(scene.get());
}

// Tests encoding a simple glTF with Cesium RTC.
TEST_F(GltfEncoderTest, EncodeWithCesiumRtc) {
  const std::string file_name = "Box/glTF/Box.gltf";
  const std::unique_ptr<Scene> scene(DecodeTestGltfFileToScene(file_name));
  ASSERT_NE(scene, nullptr);
  scene->SetCesiumRtc(std::vector<double>({123.4, -234.5, -45678.9}));
  EncodeSceneToGltfAndCompare(scene.get());
}

// Tests encoding a simple glTF with Draco compression.
TEST_F(GltfEncoderTest, EncodeWithDracoCompression) {
  const std::string file_name = "Box/glTF/Box.gltf";
  const std::unique_ptr<Scene> scene(DecodeTestGltfFileToScene(file_name));
  ASSERT_NE(scene, nullptr);
  const DracoCompressionOptions options;
  SceneUtils::SetDracoCompressionOptions(&options, scene.get());
  EncodeSceneToGltfAndCompare(scene.get());
}

TEST_F(GltfEncoderTest, EncodeWeightsJointsWithDracoCompression) {
  const std::string file_name = "CesiumMan/glTF/CesiumMan.gltf";
  const std::unique_ptr<Scene> scene(DecodeTestGltfFileToScene(file_name));
  ASSERT_NE(scene, nullptr);
  const DracoCompressionOptions options;
  SceneUtils::SetDracoCompressionOptions(&options, scene.get());
  EncodeSceneToGltfAndCompare(scene.get());
}

TEST_F(GltfEncoderTest, EncodeTangentsWithDracoCompression) {
  const std::string file_name = "Lantern/glTF/Lantern.gltf";
  const std::unique_ptr<Scene> scene(DecodeTestGltfFileToScene(file_name));
  ASSERT_NE(scene, nullptr);
  const DracoCompressionOptions options;
  SceneUtils::SetDracoCompressionOptions(&options, scene.get());
  EncodeSceneToGltfAndCompare(scene.get());
}

TEST_F(GltfEncoderTest, TestDracoCompressionWithGeneratedPoints) {
  const std::string basename = "test_nm.obj";
  std::unique_ptr<draco::Mesh> mesh = draco::ReadMeshFromTestFile(basename);
  ASSERT_NE(mesh, nullptr) << "Failed to load " << basename;

  auto maybe_scene = draco::SceneUtils::MeshToScene(std::move(mesh));
  ASSERT_TRUE(maybe_scene.ok()) << "Failed Mesh to Scene conversion.";
  const std::unique_ptr<Scene> scene = std::move(maybe_scene).value();
  ASSERT_NE(scene, nullptr);
  const DracoCompressionOptions options;
  SceneUtils::SetDracoCompressionOptions(&options, scene.get());
  EncodeSceneToGltfAndCompare(scene.get());
}

TEST_F(GltfEncoderTest, TestDracoCompressionWithDegenerateFaces) {
  const std::string basename = "deg_faces.obj";
  std::unique_ptr<draco::Mesh> mesh = draco::ReadMeshFromTestFile(basename);
  ASSERT_NE(mesh, nullptr) << "Failed to load " << basename;
  ASSERT_EQ(mesh->num_faces(), 4);

  auto maybe_scene = draco::SceneUtils::MeshToScene(std::move(mesh));
  ASSERT_TRUE(maybe_scene.ok()) << "Failed Mesh to Scene conversion.";
  const std::unique_ptr<Scene> scene = std::move(maybe_scene).value();
  ASSERT_NE(scene, nullptr);
  const Mesh& scene_first_mesh = scene->GetMesh(MeshIndex(0));
  ASSERT_EQ(scene_first_mesh.num_faces(), 4);

  std::unique_ptr<Scene> scene_from_gltf;
  const DracoCompressionOptions options;
  SceneUtils::SetDracoCompressionOptions(&options, scene.get());
  SceneToDecodedGltfScene(*scene, &scene_from_gltf);
  const Mesh& scene_from_gltf_first_mesh =
      scene_from_gltf->GetMesh(MeshIndex(0));
  ASSERT_EQ(scene_from_gltf_first_mesh.num_faces(), 3);

  CompareScenes(scene.get(), scene_from_gltf.get());
}

TEST_F(GltfEncoderTest, TestDracoCompressionWithFullyDegenerateMesh) {
  const std::string basename = "degenerate_mesh.obj";
  std::unique_ptr<draco::Mesh> mesh = draco::ReadMeshFromTestFile(basename);
  ASSERT_NE(mesh, nullptr) << "Failed to load " << basename;
  ASSERT_EQ(mesh->num_faces(), 1);

  auto maybe_scene = draco::SceneUtils::MeshToScene(std::move(mesh));
  ASSERT_TRUE(maybe_scene.ok()) << "Failed Mesh to Scene conversion.";
  const std::unique_ptr<Scene> scene = std::move(maybe_scene).value();
  ASSERT_NE(scene, nullptr);
  const Mesh& scene_first_mesh = scene->GetMesh(MeshIndex(0));
  ASSERT_EQ(scene_first_mesh.num_faces(), 1);

  std::unique_ptr<Scene> scene_from_gltf;
  const DracoCompressionOptions options;
  SceneUtils::SetDracoCompressionOptions(&options, scene.get());
  SceneToDecodedGltfScene(*scene, &scene_from_gltf);
  const Mesh& scene_from_gltf_first_mesh =
      scene_from_gltf->GetMesh(MeshIndex(0));
  ASSERT_EQ(scene_from_gltf_first_mesh.num_faces(), 1);

  CompareScenes(scene.get(), scene_from_gltf.get());
}

TEST_F(GltfEncoderTest, DracoCompressionCheckOptions) {
  const std::string file_name = "CesiumMan/glTF/CesiumMan.gltf";
  const std::unique_ptr<Scene> scene(DecodeTestGltfFileToScene(file_name));
  ASSERT_NE(scene, nullptr);

  const std::string gltf_file_full_path =
      draco::GetTestTempFileFullPath("test.gltf");
  std::string folder_path;
  std::string gltf_file_name;
  draco::SplitPath(gltf_file_full_path, &folder_path, &gltf_file_name);
  GltfEncoder gltf_encoder;
  DracoCompressionOptions options;
  SceneUtils::SetDracoCompressionOptions(&options, scene.get());

  ASSERT_TRUE(gltf_encoder.EncodeToFile<Scene>(*scene, gltf_file_full_path,
                                               folder_path))
      << "Failed gltf_file_full_path:" << gltf_file_full_path
      << " folder_path:" << folder_path;

  const std::string gltf_bin_filename =
      draco::GetTestTempFileFullPath("buffer0.bin");
  const size_t default_bin_size = draco::GetFileSize(gltf_bin_filename);

  // Test applying more quantization will make the compressed size smaller.
  options.quantization_position.SetQuantizationBits(6);
  options.quantization_bits_normal = 6;
  options.quantization_bits_tex_coord = 6;
  SceneUtils::SetDracoCompressionOptions(&options, scene.get());

  ASSERT_TRUE(gltf_encoder.EncodeToFile<Scene>(*scene, gltf_file_full_path,
                                               folder_path))
      << "Failed gltf_file_full_path:" << gltf_file_full_path
      << " folder_path:" << folder_path;
  const size_t more_quantization_bin_size =
      draco::GetFileSize(gltf_bin_filename);
  ASSERT_LT(more_quantization_bin_size, default_bin_size);

  // Test setting more weight quantization then the default makes the compressed
  // size smaller.
  options.quantization_bits_weight = 6;
  SceneUtils::SetDracoCompressionOptions(&options, scene.get());

  ASSERT_TRUE(gltf_encoder.EncodeToFile<Scene>(*scene, gltf_file_full_path,
                                               folder_path))
      << "Failed gltf_file_full_path:" << gltf_file_full_path
      << " folder_path:" << folder_path;
  const size_t more_weight_quantization_bin_size =
      draco::GetFileSize(gltf_bin_filename);
  ASSERT_LT(more_weight_quantization_bin_size, more_quantization_bin_size);

  options.quantization_position.SetQuantizationBits(20);
  options.quantization_bits_normal = 20;
  options.quantization_bits_tex_coord = 20;
  options.quantization_bits_weight = 20;
  SceneUtils::SetDracoCompressionOptions(&options, scene.get());

  ASSERT_TRUE(gltf_encoder.EncodeToFile<Scene>(*scene, gltf_file_full_path,
                                               folder_path))
      << "Failed gltf_file_full_path:" << gltf_file_full_path
      << " folder_path:" << folder_path;
  const size_t less_quantization_bin_size =
      draco::GetFileSize(gltf_bin_filename);
  ASSERT_GT(less_quantization_bin_size, default_bin_size);

  DracoCompressionOptions level_options;
  level_options.compression_level = 10;  // compression level [0-10].
  SceneUtils::SetDracoCompressionOptions(&level_options, scene.get());
  ASSERT_TRUE(gltf_encoder.EncodeToFile<Scene>(*scene, gltf_file_full_path,
                                               folder_path))
      << "Failed gltf_file_full_path:" << gltf_file_full_path
      << " folder_path:" << folder_path;
  const size_t most_compression_bin_size =
      draco::GetFileSize(gltf_bin_filename);
  ASSERT_LT(most_compression_bin_size, default_bin_size);

  level_options.compression_level = 4;
  SceneUtils::SetDracoCompressionOptions(&level_options, scene.get());
  ASSERT_TRUE(gltf_encoder.EncodeToFile<Scene>(*scene, gltf_file_full_path,
                                               folder_path))
      << "Failed gltf_file_full_path:" << gltf_file_full_path
      << " folder_path:" << folder_path;
  const size_t less_compression_bin_size =
      draco::GetFileSize(gltf_bin_filename);
  ASSERT_GT(less_compression_bin_size, default_bin_size);

  level_options.compression_level = 0;
  SceneUtils::SetDracoCompressionOptions(&level_options, scene.get());
  ASSERT_TRUE(gltf_encoder.EncodeToFile<Scene>(*scene, gltf_file_full_path,
                                               folder_path))
      << "Failed gltf_file_full_path:" << gltf_file_full_path
      << " folder_path:" << folder_path;
  const size_t least_compression_bin_size =
      draco::GetFileSize(gltf_bin_filename);
  ASSERT_GT(least_compression_bin_size, less_compression_bin_size);
}

TEST_F(GltfEncoderTest, TestQuantizationPerAttribute) {
  const std::string file_name = "sphere.gltf";
  const std::unique_ptr<Scene> scene(DecodeTestGltfFileToScene(file_name));
  ASSERT_NE(scene, nullptr);

  const std::string gltf_file_full_path =
      draco::GetTestTempFileFullPath("test.gltf");
  std::string folder_path;
  std::string gltf_file_name;
  draco::SplitPath(gltf_file_full_path, &folder_path, &gltf_file_name);
  GltfEncoder gltf_encoder;
  DracoCompressionOptions options;
  SceneUtils::SetDracoCompressionOptions(&options, scene.get());

  ASSERT_TRUE(gltf_encoder.EncodeToFile<Scene>(*scene, gltf_file_full_path,
                                               folder_path))
      << "Failed gltf_file_full_path:" << gltf_file_full_path
      << " folder_path:" << folder_path;

  const std::string gltf_bin_filename =
      draco::GetTestTempFileFullPath("buffer0.bin");
  const size_t default_bin_size = draco::GetFileSize(gltf_bin_filename);

  // Test setting more position quantization then the default makes the
  // compressed size smaller.
  options.quantization_position.SetQuantizationBits(6);
  SceneUtils::SetDracoCompressionOptions(&options, scene.get());
  ASSERT_TRUE(gltf_encoder.EncodeToFile<Scene>(*scene, gltf_file_full_path,
                                               folder_path))
      << "Failed gltf_file_full_path:" << gltf_file_full_path
      << " folder_path:" << folder_path;
  const size_t position_quantization_bin_size =
      draco::GetFileSize(gltf_bin_filename);
  ASSERT_LT(position_quantization_bin_size, default_bin_size);

  // Test setting more normal quantization then the default makes the compressed
  // size smaller.
  options.quantization_bits_normal = 6;
  SceneUtils::SetDracoCompressionOptions(&options, scene.get());
  ASSERT_TRUE(gltf_encoder.EncodeToFile<Scene>(*scene, gltf_file_full_path,
                                               folder_path))
      << "Failed gltf_file_full_path:" << gltf_file_full_path
      << " folder_path:" << folder_path;
  const size_t normal_quantization_bin_size =
      draco::GetFileSize(gltf_bin_filename);
  ASSERT_LT(normal_quantization_bin_size, position_quantization_bin_size);

  // Test setting more tex_coord quantization then the default makes the
  // compressed size smaller.
  options.quantization_bits_tex_coord = 6;
  SceneUtils::SetDracoCompressionOptions(&options, scene.get());
  ASSERT_TRUE(gltf_encoder.EncodeToFile<Scene>(*scene, gltf_file_full_path,
                                               folder_path))
      << "Failed gltf_file_full_path:" << gltf_file_full_path
      << " folder_path:" << folder_path;
  const size_t tex_coord_quantization_bin_size =
      draco::GetFileSize(gltf_bin_filename);
  ASSERT_LT(tex_coord_quantization_bin_size, normal_quantization_bin_size);

  // Test setting more tangent quantization then the default makes the
  // compressed size smaller. Weight is tested in DracoCompressionCheckOptions.
  options.quantization_bits_tangent = 6;
  SceneUtils::SetDracoCompressionOptions(&options, scene.get());
  ASSERT_TRUE(gltf_encoder.EncodeToFile<Scene>(*scene, gltf_file_full_path,
                                               folder_path))
      << "Failed gltf_file_full_path:" << gltf_file_full_path
      << " folder_path:" << folder_path;
  const size_t tangent_quantization_bin_size =
      draco::GetFileSize(gltf_bin_filename);
  ASSERT_LT(tangent_quantization_bin_size, tex_coord_quantization_bin_size);
}

// Tests encoding a glTF with multiple scaled instances with Draco compression
// using grid options for position quantization.
TEST_F(GltfEncoderTest, TestDracoCompressionWithGridOptions) {
  const std::string file_name =
      "SpheresScaledInstances/glTF/spheres_scaled_instances.gltf";
  std::unique_ptr<Scene> scene(DecodeTestGltfFileToScene(file_name));
  ASSERT_NE(scene, nullptr);

  const auto bbox = scene->GetMesh(MeshIndex(0)).ComputeBoundingBox();
  const float mesh_size = bbox.Size().MaxCoeff();

  // All dimensions of the original mesh are between [-1, 1]. Let's move the
  // mesh to [0, 2] which will allow us to match grid quantization with the
  // regular quantization (grid quantization is always aligned with 0).
  Mesh& mesh = scene->GetMesh(MeshIndex(0));
  PointAttribute* pos_att =
      mesh.attribute(mesh.GetNamedAttributeId(GeometryAttribute::POSITION));
  for (AttributeValueIndex avi(0); avi < pos_att->size(); ++avi) {
    Vector3f pos;
    pos_att->GetValue(avi, &pos[0]);
    pos += Vector3f(1.f, 1.f, 1.f);
    pos_att->SetAttributeValue(avi, &pos[0]);
  }

  DracoCompressionOptions options;

  // First quantize the scene with 8 bits and save the result.
  options.quantization_position.SetQuantizationBits(8);
  SceneUtils::SetDracoCompressionOptions(&options, scene.get());

  const std::string gltf_filename = draco::GetTestTempFileFullPath("temp.glb");
  GltfEncoder encoder;
  DRACO_ASSERT_OK(encoder.EncodeFile(*scene, gltf_filename));
  // Get the size of the generated file.
  const size_t qb_file_size = draco::GetFileSize(gltf_filename);

  // Now set grid quantization and ensure the encoded file size is about the
  // same. The max instance scale is 3 and model size is |mesh_size| so the grid
  // scale must account for that.
  options.quantization_position.SetGrid(mesh_size * 3. / 255.);
  SceneUtils::SetDracoCompressionOptions(&options, scene.get());

  DRACO_ASSERT_OK(encoder.EncodeFile(*scene, gltf_filename));
  // Get the size of the generated file.
  const size_t grid_file_size = draco::GetFileSize(gltf_filename);

  ASSERT_EQ(grid_file_size, qb_file_size);

  // Now set grid quantization to different settings and ensure the encoded size
  // changed. We reduce spacing which should increase the size.
  options.quantization_position.SetGrid(mesh_size * 3. / 511.);
  SceneUtils::SetDracoCompressionOptions(&options, scene.get());

  DRACO_ASSERT_OK(encoder.EncodeFile(*scene, gltf_filename));

  // Get the size of the generated file.
  const size_t grid_file_size_2 = draco::GetFileSize(gltf_filename);
  ASSERT_GT(grid_file_size_2, grid_file_size);
}

TEST_F(GltfEncoderTest, TestOutputType) {
  const std::string file_name = "sphere.gltf";
  const std::unique_ptr<Scene> scene(DecodeTestGltfFileToScene(file_name));
  ASSERT_NE(scene, nullptr);

  const std::string gltf_file_full_path =
      draco::GetTestTempFileFullPath("test.gltf");
  std::string folder_path;
  std::string gltf_file_name;
  draco::SplitPath(gltf_file_full_path, &folder_path, &gltf_file_name);
  GltfEncoder gltf_encoder;

  ASSERT_TRUE(gltf_encoder.EncodeToFile<Scene>(*scene, gltf_file_full_path,
                                               folder_path))
      << "Failed gltf_file_full_path:" << gltf_file_full_path
      << " folder_path:" << folder_path;

  const size_t default_gltf_size = draco::GetFileSize(gltf_file_full_path);

  // Test setting VERBOSE output type will increase the size of the gltf file.
  gltf_encoder.set_output_type(GltfEncoder::VERBOSE);
  ASSERT_TRUE(gltf_encoder.EncodeToFile<Scene>(*scene, gltf_file_full_path,
                                               folder_path))
      << "Failed gltf_file_full_path:" << gltf_file_full_path
      << " folder_path:" << folder_path;
  const size_t verbose_gltf_size = draco::GetFileSize(gltf_file_full_path);
  ASSERT_GT(verbose_gltf_size, default_gltf_size);
}

// Tests copying the name of the input texture file to the encoded texture file.
TEST_F(GltfEncoderTest, CopyTextureName) {
  std::unique_ptr<draco::Mesh> mesh =
      draco::ReadMeshFromTestFile("CesiumMilkTruck/glTF/CesiumMilkTruck.gltf");
  ASSERT_NE(mesh, nullptr);

  std::unique_ptr<Mesh> mesh_from_gltf;
  MeshToDecodedGltfMesh(*mesh, &mesh_from_gltf);
  const Material* material = mesh->GetMaterialLibrary().GetMaterial(0);
  ASSERT_NE(material, nullptr);
  const Texture* texture =
      mesh->GetMaterialLibrary().GetTextureLibrary().GetTexture(0);
  ASSERT_NE(texture, nullptr);
  ASSERT_EQ(draco::TextureUtils::GetTargetStem(*texture), "CesiumMilkTruck");
  ASSERT_EQ(draco::TextureUtils::GetTargetFormat(*texture),
            draco::ImageFormat::PNG);
}

TEST_F(GltfEncoderTest, EncodeTexCoord1) {
  std::unique_ptr<draco::Mesh> mesh =
      draco::ReadMeshFromTestFile("MultiUVTest/glTF/MultiUVTest.gltf");

  std::unique_ptr<Mesh> mesh_from_gltf;
  MeshToDecodedGltfMesh(*mesh, &mesh_from_gltf);
  ASSERT_EQ(mesh_from_gltf->GetMaterialLibrary().NumMaterials(), 1);
  ASSERT_EQ(
      mesh_from_gltf->GetMaterialLibrary().GetMaterial(0)->NumTextureMaps(), 2);
  ASSERT_EQ(
      mesh_from_gltf->GetMaterialLibrary().GetTextureLibrary().NumTextures(),
      2);
  const std::vector<const draco::Texture*> textures = {
      mesh_from_gltf->GetMaterialLibrary().GetTextureLibrary().GetTexture(0),
      mesh_from_gltf->GetMaterialLibrary().GetTextureLibrary().GetTexture(1)};
  EXPECT_EQ(draco::TextureUtils::GetTargetStem(*textures[0]), "uv0");
  EXPECT_EQ(draco::TextureUtils::GetTargetStem(*textures[1]), "uv1");
  EXPECT_EQ(draco::TextureUtils::GetTargetFormat(*textures[0]),
            draco::ImageFormat::PNG);
  EXPECT_EQ(draco::TextureUtils::GetTargetFormat(*textures[1]),
            draco::ImageFormat::PNG);
  ASSERT_EQ(mesh_from_gltf->NumNamedAttributes(GeometryAttribute::TEX_COORD),
            2);
  ASSERT_EQ(mesh_from_gltf->NumNamedAttributes(GeometryAttribute::POSITION), 1);
  ASSERT_EQ(mesh_from_gltf->NumNamedAttributes(GeometryAttribute::NORMAL), 1);
  ASSERT_EQ(mesh_from_gltf->NumNamedAttributes(GeometryAttribute::TANGENT), 1);
}

TEST_F(GltfEncoderTest, TestEncodeFileFunctions) {
  const std::string file_name = "sphere.gltf";
  const std::unique_ptr<Scene> scene(DecodeTestGltfFileToScene(file_name));
  ASSERT_NE(scene, nullptr);

  // Test encoding with only the gltf filename parameter will output the correct
  // bin filename and the textures will be in the same directory as the output
  // glTF file.
  const std::string output_gltf_filename =
      draco::GetTestTempFileFullPath("encoded_example.gltf");
  std::string output_gltf_dir;
  std::string output_gltf_basename;
  draco::SplitPath(output_gltf_filename, &output_gltf_dir,
                   &output_gltf_basename);

  GltfEncoder gltf_encoder;
  ASSERT_TRUE(gltf_encoder.EncodeFile<Scene>(*scene, output_gltf_filename).ok())
      << "Failed to encode glTF filename:" << output_gltf_filename;

  const std::string output_bin_filename =
      draco::GetTestTempFileFullPath("encoded_example.bin");
  const size_t output_bin_size = draco::GetFileSize(output_bin_filename);
  ASSERT_GT(output_bin_size, 0);
  const std::string output_png_filename =
      draco::GetTestTempFileFullPath("sphere_Texture0_Normal.png");
  const size_t output_png_size = draco::GetFileSize(output_png_filename);
  ASSERT_GT(output_png_size, 0);

  // Test encoding with the gltf and bin filename parameter, the textures will
  // be in the same directory as the output glTF file.
  const std::string new_bin_filename =
      draco::GetTestTempFileFullPath("different_stem_name.bin");
  ASSERT_TRUE(
      gltf_encoder
          .EncodeFile<Scene>(*scene, output_gltf_filename, new_bin_filename)
          .ok())
      << "Failed to encode glTF filename:" << output_gltf_filename;

  const size_t new_bin_size = draco::GetFileSize(new_bin_filename);
  ASSERT_GT(new_bin_size, 0);
  ASSERT_EQ(new_bin_size, output_bin_size);

  // Test encoding with the gltf and bin filename and resource_dir parameter,
  // the textures will be in the resource_dir directory.
  const std::string new_resource_dir = output_gltf_dir + "/textures";
  ASSERT_TRUE(gltf_encoder
                  .EncodeFile<Scene>(*scene, output_gltf_filename,
                                     new_bin_filename, new_resource_dir)
                  .ok())
      << "Failed to encode glTF filename:" << output_gltf_filename;

  const std::string new_png_filename =
      draco::GetTestTempFileFullPath("textures/sphere_Texture0_Normal.png");
  const size_t newest_bin_size = draco::GetFileSize(new_bin_filename);
  ASSERT_GT(new_bin_size, 0);
  ASSERT_EQ(new_bin_size, output_bin_size);
  ASSERT_EQ(newest_bin_size, new_bin_size);
  const size_t new_png_size = draco::GetFileSize(new_png_filename);
  ASSERT_GT(new_png_size, 0);
  ASSERT_EQ(new_png_size, output_png_size);
}

TEST_F(GltfEncoderTest, DoubleSidedMaterial) {
  const std::string file_name = "TwoSidedPlane/glTF/TwoSidedPlane.gltf";
  const std::unique_ptr<Scene> scene(DecodeTestGltfFileToScene(file_name));
  ASSERT_NE(scene, nullptr);
  EXPECT_EQ(scene->GetMaterialLibrary().NumMaterials(), 1);
  EXPECT_EQ(scene->GetMaterialLibrary().GetMaterial(0)->GetDoubleSided(), true);

  std::unique_ptr<Scene> scene_from_gltf;
  SceneToDecodedGltfScene(*scene, &scene_from_gltf);
  EXPECT_EQ(scene_from_gltf->GetMaterialLibrary().NumMaterials(), 1);
  EXPECT_EQ(
      scene_from_gltf->GetMaterialLibrary().GetMaterial(0)->GetDoubleSided(),
      true);
}

TEST_F(GltfEncoderTest, EncodeGlb) {
  const std::string file_name = "sphere.gltf";
  const std::unique_ptr<Scene> scene(DecodeTestGltfFileToScene(file_name));
  ASSERT_NE(scene, nullptr);

  std::unique_ptr<Scene> scene_from_gltf;
  SceneToDecodedGltfScene(*scene, "temp.gltf", &scene_from_gltf);

  std::unique_ptr<Scene> scene_from_glb;
  SceneToDecodedGltfScene(*scene, "temp.glb", &scene_from_glb);

  CompareScenes(scene_from_gltf.get(), scene_from_glb.get());
}

TEST_F(GltfEncoderTest, EncodeVertexColor) {
  const std::string file_name = "VertexColorTest/glTF/VertexColorTest.gltf";
  const std::unique_ptr<Scene> scene(DecodeTestGltfFileToScene(file_name));
  ASSERT_NE(scene, nullptr);
  EXPECT_EQ(scene->NumMeshes(), 2);
  const Mesh& mesh = scene->GetMesh(MeshIndex(1));
  EXPECT_EQ(mesh.NumNamedAttributes(GeometryAttribute::COLOR), 1);

  std::unique_ptr<Scene> scene_from_gltf;
  SceneToDecodedGltfScene(*scene, "temp.gltf", &scene_from_gltf);
  EXPECT_EQ(scene_from_gltf->NumMeshes(), 2);
  const Mesh& encoded_mesh = scene_from_gltf->GetMesh(MeshIndex(1));
  EXPECT_EQ(encoded_mesh.NumNamedAttributes(GeometryAttribute::COLOR), 1);
}

TEST_F(GltfEncoderTest, InterpolationTest) {
  const std::string file_name = "InterpolationTest/glTF/InterpolationTest.gltf";
  const std::unique_ptr<Scene> scene(DecodeTestGltfFileToScene(file_name));
  ASSERT_NE(scene, nullptr);

  std::unique_ptr<Scene> transcoded_scene;
  SceneToDecodedGltfScene(*scene, "InterpolationTest.gltf", &transcoded_scene);
  ASSERT_NE(transcoded_scene, nullptr);
  CompareScenes(scene.get(), transcoded_scene.get());
  EXPECT_EQ(transcoded_scene->NumAnimations(), 9);

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

  // Currently all animation data is unique. See b/145703399.
  const int num_input_accessors = 9;
  const int num_output_accessors = 9;
  CheckAnimationAccessors(*transcoded_scene, num_input_accessors,
                          num_output_accessors);
}

TEST_F(GltfEncoderTest, KhrMaterialUnlit) {
  const std::string filename =
      "KhronosSampleModels/UnlitTest/glTF/UnlitTest.gltf";
  const std::unique_ptr<Scene> scene(DecodeTestGltfFileToScene(filename));
  ASSERT_NE(scene, nullptr);

  const std::string output_gltf_filename =
      draco::GetTestTempFileFullPath("encoded_example.gltf");
  std::string output_gltf_dir;
  std::string output_gltf_basename;
  draco::SplitPath(output_gltf_filename, &output_gltf_dir,
                   &output_gltf_basename);

  GltfEncoder gltf_encoder;
  ASSERT_TRUE(gltf_encoder.EncodeFile<Scene>(*scene, output_gltf_filename).ok())
      << "Failed to encode glTF filename:" << output_gltf_filename;
  // glTF file should have four occurrences of "KHR_materials_unlit". Two in
  // the materials and one in extensionsUsed and one in extensionsRequired.
  CheckGltfFileAtLeastStringCount(output_gltf_filename, "KHR_materials_unlit",
                                  4);
}

TEST_F(GltfEncoderTest, OneMaterialUnlitWithFallback) {
  const std::string filename =
      "UnlitWithFallback/one_material_all_fallback/"
      "one_material_all_fallback.gltf";
  const std::unique_ptr<Scene> scene(DecodeTestGltfFileToScene(filename));
  ASSERT_NE(scene, nullptr);

  const std::string output_gltf_filename =
      draco::GetTestTempFileFullPath("encoded_example.gltf");
  std::string output_gltf_dir;
  std::string output_gltf_basename;
  draco::SplitPath(output_gltf_filename, &output_gltf_dir,
                   &output_gltf_basename);

  GltfEncoder gltf_encoder;
  ASSERT_TRUE(gltf_encoder.EncodeFile<Scene>(*scene, output_gltf_filename).ok())
      << "Failed to encode glTF filename:" << output_gltf_filename;

  // glTF file should have two occurrences of "KHR_materials_unlit". One in
  // the materials and one in extensionsUsed.
  CheckGltfFileAtLeastStringCount(output_gltf_filename, "KHR_materials_unlit",
                                  2);

  // The glTF file should provide a fallback to "KHR_materials_unlit", so there
  // should be no "extensionsRequired" element.
  CheckGltfFileNoString(output_gltf_filename, "extensionsRequired");
}

TEST_F(GltfEncoderTest, MultipleMaterialsUnlitWithFallback) {
  std::string filename =
      "UnlitWithFallback/three_materials_all_fallback/"
      "three_materials_all_fallback.gltf";
  const std::unique_ptr<Scene> scene_all_fallback(
      DecodeTestGltfFileToScene(filename));
  ASSERT_NE(scene_all_fallback, nullptr);

  const std::string output_gltf_filename =
      draco::GetTestTempFileFullPath("encoded_example.gltf");
  std::string output_gltf_dir;
  std::string output_gltf_basename;
  draco::SplitPath(output_gltf_filename, &output_gltf_dir,
                   &output_gltf_basename);

  GltfEncoder gltf_encoder;
  ASSERT_TRUE(
      gltf_encoder.EncodeFile<Scene>(*scene_all_fallback, output_gltf_filename)
          .ok())
      << "Failed to encode glTF filename:" << output_gltf_filename;

  // glTF file should have four occurrences of "KHR_materials_unlit". Three in
  // the materials and one in extensionsUsed.
  CheckGltfFileAtLeastStringCount(output_gltf_filename, "KHR_materials_unlit",
                                  4);

  // The glTF file should provide a fallback to "KHR_materials_unlit", so there
  // should be no "extensionsRequired" element.
  CheckGltfFileNoString(output_gltf_filename, "extensionsRequired");

  filename =
      "UnlitWithFallback/three_materials_one_fallback/"
      "three_materials_one_fallback.gltf";
  const std::unique_ptr<Scene> scene_one_fallback(
      DecodeTestGltfFileToScene(filename));
  ASSERT_NE(scene_one_fallback, nullptr);

  ASSERT_TRUE(
      gltf_encoder.EncodeFile<Scene>(*scene_one_fallback, output_gltf_filename)
          .ok())
      << "Failed to encode glTF filename:" << output_gltf_filename;

  // glTF file should have three occurrences of "KHR_materials_unlit". One in
  // the materials, one in extensionsUsed, and one in extensionsRequired.
  CheckGltfFileAtLeastStringCount(output_gltf_filename, "KHR_materials_unlit",
                                  3);

  // The glTF file only has one material with a fallback for
  // "KHR_materials_unlit". The other two materials have "KHR_materials_unlit"
  // set without a fallback, so there should be an "extensionsRequired" element.
  CheckGltfFileAtLeastStringCount(output_gltf_filename, "extensionsRequired",
                                  1);
}

TEST_F(GltfEncoderTest, KhrMaterialsSheenExtension) {
  const std::string filename =
      "KhronosSampleModels/SheenCloth/glTF/SheenCloth.gltf";
  const std::unique_ptr<Scene> scene(DecodeTestGltfFileToScene(filename));
  ASSERT_NE(scene, nullptr);

  const std::string out_filename =
      draco::GetTestTempFileFullPath("encoded_example.gltf");
  std::string output_gltf_dir;
  std::string output_gltf_basename;
  draco::SplitPath(out_filename, &output_gltf_dir, &output_gltf_basename);

  GltfEncoder gltf_encoder;
  ASSERT_TRUE(gltf_encoder.EncodeFile<Scene>(*scene, out_filename).ok())
      << "Failed to encode glTF filename:" << out_filename;

  // The "KHR_materials_sheen" should be in material and in extensionsUsed.
  CheckGltfFileAtLeastStringCount(out_filename, "KHR_materials_sheen", 2);
  CheckGltfFileAtLeastStringCount(out_filename, "sheenColorFactor", 1);
  CheckGltfFileAtLeastStringCount(out_filename, "sheenColorTexture", 1);
  CheckGltfFileAtLeastStringCount(out_filename, "sheenRoughnessFactor", 1);
  CheckGltfFileAtLeastStringCount(out_filename, "sheenRoughnessTexture", 1);
}

TEST_F(GltfEncoderTest, PbrNextExtensions) {
  // Check that a model with PBR material extensions is encoded correctly. This
  // is done by encoding an original model with all PBR material extension
  // properties and textures, then decoding it and checking that it matches the
  // original model.
  // TODO(vytyaz): Test multiple materials with various sets of extensions.

  // Read the original model.
  const std::string orig_name = "pbr_next/sphere/glTF/sphere.gltf";
  const std::unique_ptr<Scene> original(DecodeTestGltfFileToScene(orig_name));
  ASSERT_NE(original, nullptr);
  const Material& original_mat = *original->GetMaterialLibrary().GetMaterial(0);

  // Check that the original material has PBR extensions.
  EXPECT_TRUE(original_mat.HasSheen());
  EXPECT_TRUE(original_mat.HasTransmission());
  EXPECT_TRUE(original_mat.HasClearcoat());
  EXPECT_TRUE(original_mat.HasVolume());
  EXPECT_TRUE(original_mat.HasIor());
  EXPECT_TRUE(original_mat.HasSpecular());

  // Write the original model to a temporary file.
  GltfEncoder encoder;
  const std::string tmp_name = draco::GetTestTempFileFullPath("tmp.gltf");
  DRACO_ASSERT_OK(encoder.EncodeFile<Scene>(*original, tmp_name));

  // Read model from the temporary file.
  GltfDecoder decoder;
  DRACO_ASSIGN_OR_ASSERT(auto encoded, decoder.DecodeFromFileToScene(tmp_name));
  ASSERT_NE(encoded, nullptr);
}

TEST_F(GltfEncoderTest, BoxWithRequiredTransmission) {
  const std::string file_name = "Box/glTF/Box_with_required_transmission.gltf";
  const std::unique_ptr<Scene> scene(ReadSceneFromTestFile(file_name));
  ASSERT_NE(scene, nullptr);

  // Encode the scene and decode it back.
  std::unique_ptr<Scene> decoded_scene;
  SceneToDecodedGltfScene(*scene, "Box_roundtrip.gltf", &decoded_scene);
  ASSERT_NE(decoded_scene, nullptr);

  // Check that scene material has transmission.
  const MaterialLibrary& material_library = decoded_scene->GetMaterialLibrary();
  ASSERT_EQ(material_library.NumMaterials(), 1);
  const Material& material = *material_library.GetMaterial(0);
  ASSERT_TRUE(material.HasTransmission());
  ASSERT_EQ(material.GetTransmissionFactor(), 0.75f);
}

TEST_F(GltfEncoderTest, KhrTextureTransformWithoutFallback) {
  // This is the example from Khronos, which should have "KHR_texture_transform"
  // listed in the extensionsRequired, but does not for testing out client
  // implementations.
  const std::string filename =
      "KhronosSampleModels/TextureTransformTest/glTF/TextureTransformTest.gltf";
  const std::unique_ptr<Scene> scene(DecodeTestGltfFileToScene(filename));
  ASSERT_NE(scene, nullptr);

  const std::string output_gltf_filename =
      draco::GetTestTempFileFullPath("encoded_example.gltf");
  std::string output_gltf_dir;
  std::string output_gltf_basename;
  draco::SplitPath(output_gltf_filename, &output_gltf_dir,
                   &output_gltf_basename);

  GltfEncoder gltf_encoder;
  ASSERT_TRUE(gltf_encoder.EncodeFile<Scene>(*scene, output_gltf_filename).ok())
      << "Failed to encode glTF filename:" << output_gltf_filename;
  // glTF file should have eight occurrences of "KHR_texture_transform". Six in
  // the materials and one in extensionsUsed and one in extensionsRequired.
  CheckGltfFileAtLeastStringCount(output_gltf_filename, "KHR_texture_transform",
                                  8);

  // glTF file should still contain only two occurrences of `"sampler": 0`.
  CheckGltfFileAtLeastStringCount(output_gltf_filename, "\"sampler\": 0", 2);

  // glTF file should have one occurrence of "wrapS", "wrapT", "minFilter",
  // and "magFilter".
  CheckGltfFileAtLeastStringCount(output_gltf_filename, "wrapS", 1);
  CheckGltfFileAtLeastStringCount(output_gltf_filename, "wrapT", 1);
  CheckGltfFileAtLeastStringCount(output_gltf_filename, "minFilter", 1);
  CheckGltfFileAtLeastStringCount(output_gltf_filename, "magFilter", 1);
}

TEST_F(GltfEncoderTest, KhrTextureTransformWithoutFallbackRequired) {
  // This is the example from Khronos, changed to list "KHR_texture_transform"
  // in extensionsRequired.
  const std::string filename =
      "glTF/TextureTransformTestWithRequired/"
      "TextureTransformTestWithRequired.gltf";
  const std::unique_ptr<Scene> scene(DecodeTestGltfFileToScene(filename));
  ASSERT_NE(scene, nullptr);

  const std::string output_gltf_filename =
      draco::GetTestTempFileFullPath("encoded_example.gltf");
  std::string output_gltf_dir;
  std::string output_gltf_basename;
  draco::SplitPath(output_gltf_filename, &output_gltf_dir,
                   &output_gltf_basename);

  GltfEncoder gltf_encoder;
  ASSERT_TRUE(gltf_encoder.EncodeFile<Scene>(*scene, output_gltf_filename).ok())
      << "Failed to encode glTF filename:" << output_gltf_filename;
  // glTF file should have eight occurrences of "KHR_texture_transform". Six in
  // the materials and one in extensionsUsed and one in extensionsRequired.
  CheckGltfFileAtLeastStringCount(output_gltf_filename, "KHR_texture_transform",
                                  8);
}

TEST_F(GltfEncoderTest, KhrTextureTransformWithFallback) {
  // This is an example of "KHR_texture_transform" extension with fallback data.
  const std::string filename =
      "glTF/KhrTextureTransformWithFallback/"
      "KhrTextureTransformWithFallback.gltf";
  const std::unique_ptr<Scene> scene(DecodeTestGltfFileToScene(filename));
  ASSERT_NE(scene, nullptr);

  const std::string output_gltf_filename =
      draco::GetTestTempFileFullPath("encoded_example.gltf");
  std::string output_gltf_dir;
  std::string output_gltf_basename;
  draco::SplitPath(output_gltf_filename, &output_gltf_dir,
                   &output_gltf_basename);

  GltfEncoder gltf_encoder;
  ASSERT_TRUE(gltf_encoder.EncodeFile<Scene>(*scene, output_gltf_filename).ok())
      << "Failed to encode glTF filename:" << output_gltf_filename;
  // glTF file should have two occurrences of "KHR_texture_transform". One in
  // the materials and one in extensionsUsed.
  CheckGltfFileAtLeastStringCount(output_gltf_filename, "KHR_texture_transform",
                                  2);
}

// Tests if the source file has a node with an identity matrix, that we do not
// output the identity matrix.
TEST_F(GltfEncoderTest, MeshWithIdentityTransformation) {
  const std::string gltf_source_full_path =
      GetTestFileFullPath("Triangle/glTF/Triangle_identity_matrix.gltf");

  // Check that the source file contains one "matrix" and no "translation"
  // strings.
  CheckGltfFileAtLeastStringCount(gltf_source_full_path, "matrix", 1);
  CheckGltfFileNoString(gltf_source_full_path, "translation");

  std::unique_ptr<draco::Scene> scene = draco::ReadSceneFromTestFile(
      "Triangle/glTF/Triangle_identity_matrix.gltf");
  ASSERT_NE(scene, nullptr);
  SceneNode* scene_node = scene->GetNode(SceneNodeIndex(0));
  ASSERT_NE(scene_node, nullptr);
  const TrsMatrix& trs_matrix = scene_node->GetTrsMatrix();

  // gltf_decoder will not set the trs matrix if the matrix is identity.
  ASSERT_FALSE(trs_matrix.MatrixSet());

  // Add the identity matrix.
  TrsMatrix trsm;
  trsm.SetMatrix(Eigen::Matrix4d::Identity());
  scene_node->SetTrsMatrix(trsm);

  const TrsMatrix& check_trs_matrix = scene_node->GetTrsMatrix();
  ASSERT_TRUE(check_trs_matrix.MatrixSet());
  ASSERT_EQ(check_trs_matrix.IsMatrixIdentity(), true);

  const std::string gltf_file_full_path =
      draco::GetTestTempFileFullPath("MeshWithIdentityTransformation.gltf");
  std::string folder_path;
  std::string gltf_file_name;
  draco::SplitPath(gltf_file_full_path, &folder_path, &gltf_file_name);
  GltfEncoder gltf_encoder;

  ASSERT_TRUE(gltf_encoder.EncodeToFile<Scene>(
      *scene.get(), gltf_file_full_path, folder_path))
      << "Failed gltf_file_full_path:" << gltf_file_full_path
      << " folder_path:" << folder_path;
  std::unique_ptr<Scene> scene_gltf =
      std::move(ReadSceneFromFile(gltf_file_full_path)).value();
  ASSERT_NE(scene_gltf, nullptr);
  // Check that the output file contains no "matrix" or "translation" strings.
  CheckGltfFileNoString(gltf_file_full_path, "matrix");
  CheckGltfFileNoString(gltf_file_full_path, "translation");
}

// Tests if the source file has a node with a matrix that only has the
// translation values set. If it does then instead of outputting the full matrix
// we only output the "translation" glTF element.
TEST_F(GltfEncoderTest, MeshWithTranslationOnlyMatrix) {
  std::unique_ptr<draco::Scene> scene = draco::ReadSceneFromTestFile(
      "Triangle/glTF/Triangle_translation_only_matrix.gltf");
  ASSERT_NE(scene, nullptr);
  SceneNode* scene_node = scene->GetNode(SceneNodeIndex(0));
  ASSERT_NE(scene_node, nullptr);
  const TrsMatrix& input_trs_matrix = scene_node->GetTrsMatrix();
  ASSERT_TRUE(input_trs_matrix.MatrixSet());
  ASSERT_FALSE(input_trs_matrix.TranslationSet());
  ASSERT_FALSE(input_trs_matrix.RotationSet());
  ASSERT_FALSE(input_trs_matrix.ScaleSet());
  ASSERT_TRUE(input_trs_matrix.IsMatrixTranslationOnly());

  const std::string gltf_file_full_path =
      draco::GetTestTempFileFullPath("MeshWithTranslationOnlyMatrix.gltf");
  std::string folder_path;
  std::string gltf_file_name;
  draco::SplitPath(gltf_file_full_path, &folder_path, &gltf_file_name);
  GltfEncoder gltf_encoder;

  ASSERT_TRUE(gltf_encoder.EncodeToFile<Scene>(
      *scene.get(), gltf_file_full_path, folder_path))
      << "Failed gltf_file_full_path:" << gltf_file_full_path
      << " folder_path:" << folder_path;
  std::unique_ptr<Scene> scene_gltf =
      std::move(ReadSceneFromFile(gltf_file_full_path)).value();
  ASSERT_NE(scene_gltf, nullptr);
  SceneNode* output_scene_node = scene_gltf->GetNode(SceneNodeIndex(0));
  ASSERT_NE(output_scene_node, nullptr);
  const TrsMatrix& output_trs_matrix = output_scene_node->GetTrsMatrix();
  ASSERT_FALSE(output_trs_matrix.MatrixSet());
  ASSERT_TRUE(output_trs_matrix.TranslationSet());
  ASSERT_FALSE(output_trs_matrix.RotationSet());
  ASSERT_FALSE(output_trs_matrix.ScaleSet());
}

// Tests that a scene can be encoded to buffer in GLB format.
TEST_F(GltfEncoderTest, EncodeToBuffer) {
  // Load scene from file.
  const std::string file_name = "CesiumMilkTruck/glTF/CesiumMilkTruck.gltf";
  const std::unique_ptr<Scene> scene = ReadSceneFromTestFile(file_name);
  ASSERT_NE(scene, nullptr);

  // Encode scene to buffer in GLB format.
  GltfEncoder encoder;
  EncoderBuffer buffer;
  DRACO_ASSERT_OK(encoder.EncodeToBuffer(*scene, &buffer));
  ASSERT_NE(buffer.size(), 0);

  // Write scene to file in GLB format.
  const std::string glb_file_path = draco::GetTestTempFileFullPath("temp.glb");
  std::string folder_path;
  std::string glb_file_name;
  draco::SplitPath(glb_file_path, &folder_path, &glb_file_name);
  encoder.EncodeToFile<Scene>(*scene, glb_file_path, folder_path);

  // Check that the buffer contents match the GLB file contents.
  ASSERT_EQ(buffer.size(), draco::GetFileSize(glb_file_path));
  std::vector<char> file_data;
  ASSERT_TRUE(ReadFileToBuffer(glb_file_path, &file_data));
  ASSERT_EQ(std::memcmp(file_data.data(), buffer.data(), buffer.size()), 0);
}

TEST_F(GltfEncoderTest, CopyrightAssetIsEncoded) {
  // Load scene from file.
  const std::string file_name = "CesiumMilkTruck/glTF/CesiumMilkTruck.gltf";
  const std::unique_ptr<Scene> scene = ReadSceneFromTestFile(file_name);
  ASSERT_NE(scene, nullptr);

  std::array<std::pair<std::string, std::string>, 3> test_cases = {
      {{"Google", "Google"}, {"", ""}, {"GMaps", ""}}};

  for (const std::pair<std::string, std::string>& test_case : test_cases) {
    // Encode scene to buffer in GLB format.
    GltfEncoder encoder;
    encoder.set_copyright(test_case.first);
    EncoderBuffer buffer;
    DRACO_ASSERT_OK(encoder.EncodeToBuffer(*scene, &buffer));
    ASSERT_NE(buffer.size(), 0);

    const std::string glb_file_path =
        draco::GetTestTempFileFullPath(test_case.first + "temp.glb");
    std::string folder_path;
    std::string glb_file_name;
    draco::SplitPath(glb_file_path, &folder_path, &glb_file_name);
    encoder.set_copyright(test_case.second);
    encoder.EncodeToFile<Scene>(*scene, glb_file_path, folder_path);

    std::vector<char> file_data;
    ASSERT_TRUE(ReadFileToBuffer(glb_file_path, &file_data));
    if (test_case.first == test_case.second) {
      ASSERT_EQ(buffer.size(), draco::GetFileSize(glb_file_path))
          << glb_file_path;
      ASSERT_EQ(std::memcmp(file_data.data(), buffer.data(), buffer.size()), 0);
    } else {
      ASSERT_NE(buffer.size(), draco::GetFileSize(glb_file_path))
          << glb_file_path;
    }
  }
}

// Tests that a scene with lights can be encoded into a file.
TEST_F(GltfEncoderTest, EncodeLights) {
  const std::string file_name = "sphere_lights.gltf";
  const std::unique_ptr<Scene> scene = ReadSceneFromTestFile(file_name);
  ASSERT_NE(scene, nullptr);
  ASSERT_EQ(scene->NumLights(), 4);

  EncodeSceneToGltfAndCompare(scene.get());
}

// Tests that a scene with interactivity extension can be encoded into a file.
TEST_F(GltfEncoderTest, EncodeInteractivityExtension) {
  // The KHR_interactivity extension is encoded as a pass-through extension.
  const std::string file_name = "sphere_interactivity.gltf";
  const std::unique_ptr<Scene> scene = ReadSceneFromTestFile(file_name);
  ASSERT_NE(scene, nullptr);
  ASSERT_EQ(scene->GetPassThroughExtensions().size(), 1);
  ASSERT_EQ(scene->GetPassThroughExtensions().begin()->first,
            "KHR_interactivity");
  EncodeSceneToGltfAndCompare(scene.get());
}

// Helper method for adding mesh group GPU instancing to the milk truck scene.
draco::Status AddGpuInstancingToMilkTruck(draco::Scene* scene) {
  // Create an instance and set its transformation TRS vectors.
  draco::InstanceArray::Instance instance_0;
  instance_0.trs.SetTranslation(Eigen::Vector3d(-0.2, 0.0, 0.0));
  instance_0.trs.SetScale(Eigen::Vector3d(1.0, 1.0, 1.0));

  // Create another instance.
  draco::InstanceArray::Instance instance_1;
  instance_1.trs.SetTranslation(Eigen::Vector3d(1.0, 0.0, 0.0));
  instance_1.trs.SetScale(Eigen::Vector3d(2.0, 2.0, 2.0));

  // Add an empty GPU instancing object to the scene.
  const draco::InstanceArrayIndex index = scene->AddInstanceArray();
  draco::InstanceArray* gpu_instancing = scene->GetInstanceArray(index);

  // Add two instances to the GPU instancing object stored in the scene.
  DRACO_RETURN_IF_ERROR(gpu_instancing->AddInstance(instance_0));
  DRACO_RETURN_IF_ERROR(gpu_instancing->AddInstance(instance_1));

  // Assign the GPU instancing object to two mesh groups in two scene nodes.
  scene->GetNode(draco::SceneNodeIndex(2))->SetInstanceArrayIndex(index);
  scene->GetNode(draco::SceneNodeIndex(4))->SetInstanceArrayIndex(index);

  return draco::OkStatus();
}

// Tests that a scene with instance arrays can be encoded into a file. Decoder
// has no GPU instancing support, so we will compare encoded file to a golden
// file.
TEST_F(GltfEncoderTest, EncodeInstanceArrays) {
  // Read the milk truck.
  auto scene =
      draco::ReadSceneFromTestFile("CesiumMilkTruck/glTF/CesiumMilkTruck.gltf");
  ASSERT_NE(scene, nullptr);

  // Add GPU instancing to the scene for testing.
  DRACO_ASSERT_OK(AddGpuInstancingToMilkTruck(scene.get()));
  ASSERT_EQ(scene->NumInstanceArrays(), 1);
  ASSERT_EQ(scene->NumNodes(), 5);

  // Prepare file paths.
  const std::string temp_path = draco::GetTestTempFileFullPath("Truck.glb");
  const std::string golden_path =
      GetTestFileFullPath("CesiumRowingTruckWithGpuInstancing.glb");

  // Encode scene to a temporary file in GLB format.
  std::string folder;
  std::string name;
  draco::SplitPath(temp_path, &folder, &name);
  GltfEncoder encoder;
  ASSERT_TRUE(encoder.EncodeToFile<Scene>(*scene, temp_path, folder))
      << "Failed to encode to temporary file:" << temp_path;

  // Read encoded file to buffer.
  std::vector<char> encoded_data;
  ASSERT_TRUE(ReadFileToBuffer(temp_path, &encoded_data));
}

// Tests that a scene with materials variants can be encoded into a file.
TEST_F(GltfEncoderTest, EncodeMaterialsVariants) {
  const std::string file_name =
      "KhronosSampleModels/DragonAttenuation/glTF/DragonAttenuation.gltf";
  const std::unique_ptr<Scene> scene = ReadSceneFromTestFile(file_name);
  ASSERT_NE(scene, nullptr);
  ASSERT_EQ(scene->GetMaterialLibrary().NumMaterialsVariants(), 2);
  EncodeSceneToGltfAndCompare(scene.get());
}

// Tests encoding of draco::Scene to glTF with various mesh feature ID sets,
// structural metadata property table, property attributes, and property
// textures.
TEST_F(GltfEncoderTest, EncodeSceneWithMeshFeaturesWithStructuralMetadata) {
  const std::string file_name = "BoxMeta/glTF/BoxMeta.gltf";
  GltfTestHelper::UseCase use_case;
  use_case.has_mesh_features = true;
  use_case.has_structural_metadata = true;

  // Read test file from file.
  const std::unique_ptr<Scene> scene(DecodeTestGltfFileToScene(file_name));
  ASSERT_NE(scene, nullptr);

  // Encode the scene to glTF and decode it back to draco::Scene and check.
  std::unique_ptr<Scene> scene_from_gltf;
  SceneToDecodedGltfScene(*scene, &scene_from_gltf);
  ASSERT_NE(scene_from_gltf, nullptr);
  GltfTestHelper::CheckBoxMetaMeshFeatures(*scene_from_gltf, use_case);
  GltfTestHelper::CheckBoxMetaStructuralMetadata(*scene_from_gltf, use_case);
}

// Tests encoding of draco::Scene with Draco compression to glTF with various
// mesh feature ID sets.
TEST_F(GltfEncoderTest, EncodeSceneWithMeshFeaturesWithDracoCompression) {
  const std::string file_name = "BoxMetaDraco/glTF/BoxMetaDraco.gltf";
  GltfTestHelper::UseCase use_case;
  use_case.has_draco_compression = true;
  use_case.has_mesh_features = true;

  // Read test file from file.
  const std::unique_ptr<Scene> scene(DecodeTestGltfFileToScene(file_name));
  ASSERT_NE(scene, nullptr);

  // Encode the scene to glTF and decode it back to draco::Scene and check.
  std::unique_ptr<Scene> scene_from_gltf;
  SceneToDecodedGltfScene(*scene, &scene_from_gltf);
  ASSERT_NE(scene_from_gltf, nullptr);
  GltfTestHelper::CheckBoxMetaMeshFeatures(*scene_from_gltf, use_case);
}

// Tests encoding of draco::Mesh to glTF with various mesh feature ID sets,
// structural metadata property table, property attributes, and property
// textures.
TEST_F(GltfEncoderTest, EncodeMeshWithMeshFeaturesWithStructuralMetadata) {
  const std::string file_name = "BoxMeta/glTF/BoxMeta.gltf";
  GltfTestHelper::UseCase use_case;
  use_case.has_mesh_features = true;
  use_case.has_structural_metadata = true;

  // Read test file from file.
  const std::unique_ptr<Mesh> mesh(ReadMeshFromTestFile(file_name));
  ASSERT_NE(mesh, nullptr);

  // Encode the scene to glTF and decode it back to draco::Mesh and check.
  std::unique_ptr<Mesh> mesh_from_gltf;
  MeshToDecodedGltfMesh(*mesh, &mesh_from_gltf);
  ASSERT_NE(mesh_from_gltf, nullptr);
  GltfTestHelper::CheckBoxMetaMeshFeatures(*mesh_from_gltf, use_case);
  GltfTestHelper::CheckBoxMetaStructuralMetadata(*mesh_from_gltf, use_case);
}

// Tests encoding of draco::Mesh with Draco compression to glTF with various
// mesh feature ID sets.
TEST_F(GltfEncoderTest, EncodeMeshWithMeshFeaturesWithDracoCompression) {
  const std::string file_name = "BoxMetaDraco/glTF/BoxMetaDraco.gltf";
  GltfTestHelper::UseCase use_case;
  use_case.has_draco_compression = true;
  use_case.has_mesh_features = true;

  // Read test file from file.
  const std::unique_ptr<Mesh> mesh(ReadMeshFromTestFile(file_name));
  ASSERT_NE(mesh, nullptr);

  // Encode the scene to glTF and decode it back to draco::Mesh and check.
  std::unique_ptr<Mesh> mesh_from_gltf;
  MeshToDecodedGltfMesh(*mesh, &mesh_from_gltf);
  ASSERT_NE(mesh_from_gltf, nullptr);
  GltfTestHelper::CheckBoxMetaMeshFeatures(*mesh_from_gltf, use_case);
}

// This test verifies that b/245519530 is fixed. It loads mesh with various mesh
// feature ID sets, enables Draco compression, converts mesh to scene, and
// encodes the scene to glTF.
TEST_F(GltfEncoderTest, EncodeMeshWithMeshFeaturesWithDracoCompressionAsScene) {
  // Note that although the mesh is loaded from file with no Draco compression,
  // the compression is enabled later on.
  const std::string file_name = "BoxMeta/glTF/BoxMeta.gltf";
  GltfTestHelper::UseCase use_case;
  use_case.has_draco_compression = true;
  use_case.has_mesh_features = true;
  use_case.has_structural_metadata = true;

  // Read test file from file.
  std::unique_ptr<Mesh> mesh(ReadMeshFromTestFile(file_name));
  ASSERT_NE(mesh, nullptr);

  // Enable Draco compression.
  mesh->SetCompressionEnabled(use_case.has_draco_compression);

  // Convert mesh to scene.
  std::unique_ptr<Scene> scene =
      draco::SceneUtils::MeshToScene(std::move(mesh)).value();

  // Encode the scene to glTF and decode it back to draco::Scene and check.
  std::unique_ptr<Scene> scene_from_gltf;
  SceneToDecodedGltfScene(*scene, &scene_from_gltf);
  ASSERT_NE(scene_from_gltf, nullptr);
  GltfTestHelper::CheckBoxMetaMeshFeatures(*scene_from_gltf, use_case);
}

// Tests encoding of draco::Mesh with mesh features associated with different
// mesh primitives.
TEST_F(GltfEncoderTest, EncodeMeshWithMeshFeaturesWithMultiplePrimitives) {
  const std::string file_name = "BoxesMeta/glTF/BoxesMeta.gltf";

  // Read test file from file.
  const std::unique_ptr<Mesh> mesh(ReadMeshFromTestFile(file_name));
  ASSERT_NE(mesh, nullptr);
  // Two mesh feature ID textures plus two property textures.
  ASSERT_EQ(mesh->GetNonMaterialTextureLibrary().NumTextures(), 4);

  // Encode the scene to glTF and decode it back to draco::Mesh and check.
  std::unique_ptr<Mesh> mesh_from_gltf;
  MeshToDecodedGltfMesh(*mesh, &mesh_from_gltf);
  ASSERT_NE(mesh_from_gltf, nullptr);

  ASSERT_EQ(mesh_from_gltf->GetMaterialLibrary().NumMaterials(), 2);
  ASSERT_EQ(mesh_from_gltf->NumMeshFeatures(), 5);

  // First two mesh features should be used by material 0 and the remaining by
  // material 1.
  for (draco::MeshFeaturesIndex mfi(0); mfi < 5; ++mfi) {
    // Each mesh feature should be used by a single material.
    ASSERT_EQ(mesh_from_gltf->NumMeshFeaturesMaterialMasks(mfi), 1);
    if (mfi.value() < 2) {
      ASSERT_EQ(mesh_from_gltf->GetMeshFeaturesMaterialMask(mfi, 0), 0);
    } else {
      ASSERT_EQ(mesh_from_gltf->GetMeshFeaturesMaterialMask(mfi, 0), 1);
    }
  }
  // Two mesh feature ID textures plus two property textures.
  ASSERT_EQ(mesh_from_gltf->GetNonMaterialTextureLibrary().NumTextures(), 4);

  // Ensure it still works correctly when we re-encode the source |mesh| as a
  // scene.
  std::unique_ptr<Scene> scene_from_gltf;
  MeshToDecodedGltfScene(*mesh, &scene_from_gltf);
  ASSERT_NE(scene_from_gltf, nullptr);

  ASSERT_EQ(scene_from_gltf->NumMeshes(), 2);

  // First mesh should have 2 mesh features and the other one 3 mesh features.
  ASSERT_EQ(scene_from_gltf->GetMesh(draco::MeshIndex(0)).NumMeshFeatures(), 2);
  ASSERT_EQ(scene_from_gltf->GetMesh(draco::MeshIndex(1)).NumMeshFeatures(), 3);

  // Two mesh feature ID textures plus two property textures.
  ASSERT_EQ(scene_from_gltf->GetNonMaterialTextureLibrary().NumTextures(), 4);
}

// Tests encoding of draco::Mesh with property attributes and property textures
// associated with different mesh primitives.
TEST_F(GltfEncoderTest,
       EncodeMeshWithPropertyAttributesWithMultiplePrimitives) {
  const std::string file_name = "BoxesMeta/glTF/BoxesMeta.gltf";

  // Read test file from file.
  const std::unique_ptr<Mesh> mesh(ReadMeshFromTestFile(file_name));
  ASSERT_NE(mesh, nullptr);
  ASSERT_EQ(mesh->NumPropertyAttributesIndices(), 2);
  ASSERT_EQ(mesh->NumPropertyTexturesIndices(), 2);
  ASSERT_EQ(mesh->GetNonMaterialTextureLibrary().NumTextures(), 4);

  // Encode the scene to glTF and decode it back to draco::Mesh and check.
  std::unique_ptr<Mesh> mesh_from_gltf;
  MeshToDecodedGltfMesh(*mesh, &mesh_from_gltf);
  ASSERT_NE(mesh_from_gltf, nullptr);

  ASSERT_EQ(mesh_from_gltf->GetMaterialLibrary().NumMaterials(), 2);
  ASSERT_EQ(mesh_from_gltf->NumPropertyAttributesIndices(), 2);
  ASSERT_EQ(mesh_from_gltf->NumPropertyTexturesIndices(), 2);
  ASSERT_EQ(mesh->GetNonMaterialTextureLibrary().NumTextures(), 4);

  // First property attribute should be used by material 0 and the second by
  // material 1.
  ASSERT_EQ(mesh_from_gltf->NumPropertyAttributesIndexMaterialMasks(0), 1);
  ASSERT_EQ(mesh_from_gltf->NumPropertyAttributesIndexMaterialMasks(1), 1);
  ASSERT_EQ(mesh_from_gltf->GetPropertyAttributesIndexMaterialMask(0, 0), 0);
  ASSERT_EQ(mesh_from_gltf->GetPropertyAttributesIndexMaterialMask(1, 0), 1);

  // First property texture should be used by material 0 and the second by
  // material 1.
  ASSERT_EQ(mesh_from_gltf->NumPropertyTexturesIndexMaterialMasks(0), 1);
  ASSERT_EQ(mesh_from_gltf->NumPropertyTexturesIndexMaterialMasks(1), 1);
  ASSERT_EQ(mesh_from_gltf->GetPropertyTexturesIndexMaterialMask(0, 0), 0);
  ASSERT_EQ(mesh_from_gltf->GetPropertyTexturesIndexMaterialMask(1, 0), 1);

  // Ensure it still works correctly when we re-encode the source |mesh| as a
  // scene.
  std::unique_ptr<Scene> scene_from_gltf;
  MeshToDecodedGltfScene(*mesh, &scene_from_gltf);
  ASSERT_NE(scene_from_gltf, nullptr);

  ASSERT_EQ(scene_from_gltf->NumMeshes(), 2);

  // Both meshes should have one property attributes indices.
  ASSERT_EQ(scene_from_gltf->GetMesh(draco::MeshIndex(0))
                .NumPropertyAttributesIndices(),
            1);
  ASSERT_EQ(scene_from_gltf->GetMesh(draco::MeshIndex(1))
                .NumPropertyAttributesIndices(),
            1);

  // Both meshes should have one property textures indices.
  ASSERT_EQ(scene_from_gltf->GetMesh(draco::MeshIndex(0))
                .NumPropertyTexturesIndices(),
            1);
  ASSERT_EQ(scene_from_gltf->GetMesh(draco::MeshIndex(1))
                .NumPropertyTexturesIndices(),
            1);
}

// Tests encoding of draco::Mesh containing a point cloud and two materials.
TEST_F(GltfEncoderTest, EncodePointCloudWithMaterials) {
  const std::string file_name =
      "SphereTwoMaterials/sphere_two_materials_point_cloud.gltf";

  // Read test file from file.
  const std::unique_ptr<Mesh> mesh(ReadMeshFromTestFile(file_name));
  ASSERT_NE(mesh, nullptr);

  // Input should have no faces.
  ASSERT_EQ(mesh->num_faces(), 0);

  // There should be two materials
  ASSERT_EQ(mesh->GetMaterialLibrary().NumMaterials(), 2);

  // Encode the mesh to glTF and decode it back to draco::Mesh and check.
  std::unique_ptr<Mesh> mesh_from_gltf;
  MeshToDecodedGltfMesh(*mesh, &mesh_from_gltf);
  ASSERT_NE(mesh_from_gltf, nullptr);

  ASSERT_EQ(mesh_from_gltf->num_faces(), 0);
  ASSERT_EQ(mesh_from_gltf->GetMaterialLibrary().NumMaterials(), 2);
}

// Tests encoding of draco::Mesh containing morph targets.
TEST_F(GltfEncoderTest, EncodeMeshWithMorphTargets) {
  const std::string file_name =
      "KhronosSampleModels/AnimatedMorphCube/glTF/AnimatedMorphCube.gltf";

  // Read test file from file.
  const std::unique_ptr<Scene> scene(ReadSceneFromTestFile(file_name));
  ASSERT_NE(scene, nullptr);

  // Re-encode the scene.
  draco::EncoderBuffer buffer;
  draco::GltfEncoder encoder;
  DRACO_ASSERT_OK(encoder.EncodeToBuffer(*scene, &buffer));

  // Decode again.
  draco::DecoderBuffer decoder_buffer;
  decoder_buffer.Init(buffer.data(), buffer.size());
  draco::GltfDecoder decoder;
  DRACO_ASSIGN_OR_ASSERT(std::unique_ptr<Scene> scene_from_encoded_buffer,
                         decoder.DecodeFromBufferToScene(&decoder_buffer));

  // Check that the original scene has the same morph targets as the scene
  // decoded from the encoded buffer.
  const draco::MeshIndex mi(0);
  ASSERT_EQ(scene->GetMesh(mi).num_morph_targets(),
            scene_from_encoded_buffer->GetMesh(mi).num_morph_targets());
  ASSERT_EQ(scene->GetMesh(mi).num_morph_targets(), 2);

  // Ensure all morph target attributes are the same.
  for (int mti = 0; mti < 2; ++mti) {
    ASSERT_EQ(scene->GetMesh(mi).GetMorphTarget(mti).num_attributes(),
              scene_from_encoded_buffer->GetMesh(mi).num_attributes());
    ASSERT_EQ(scene->GetMesh(mi).GetMorphTarget(mti).num_attributes(), 3);
    for (int ai = 0; ai < 3; ++ai) {
      const auto att = scene->GetMesh(mi).GetMorphTarget(mti).attribute(ai);
      const auto att_from_encoded_buffer =
          scene_from_encoded_buffer->GetMesh(mi).GetMorphTarget(mti).attribute(
              ai);
      ASSERT_EQ(att->size(), att_from_encoded_buffer->size());
      ASSERT_EQ(att->attribute_type(),
                att_from_encoded_buffer->attribute_type());
      ASSERT_EQ(att->data_type(), att_from_encoded_buffer->data_type());
      ASSERT_EQ(att->num_components(),
                att_from_encoded_buffer->num_components());

      // Check the content of the attributes.
      ASSERT_EQ(std::memcmp(att->GetAddress(draco::AttributeValueIndex(0)),
                            att_from_encoded_buffer->GetAddress(
                                draco::AttributeValueIndex(0)),
                            att->size() * att->byte_stride()),
                0);
    }
  }

  // Check weights.
  const draco::MeshGroupIndex mgi(0);
  ASSERT_EQ(scene->GetMeshGroup(mgi)->NumMorphTargetWeights(), 2);
  ASSERT_EQ(
      scene->GetMeshGroup(mgi)->NumMorphTargetWeights(),
      scene_from_encoded_buffer->GetMeshGroup(mgi)->NumMorphTargetWeights());
  for (int i = 0; i < 2; ++i) {
    ASSERT_EQ(
        scene->GetMeshGroup(mgi)->GetMorphTargetWeight(i),
        scene_from_encoded_buffer->GetMeshGroup(mgi)->GetMorphTargetWeight(i));
  }
}

TEST_F(GltfEncoderTest, TestEncodeUnboundedTexCoordAttributes) {
  // Checks that a scene with additional texture coordinate attributes
  // (TEXCOORD_2 ... TEXCOORD_7) can be successfully encoded and decoded.
  const std::string file_name = "sphere_two_tex_coords.gltf";
  const std::unique_ptr<Scene> scene(ReadSceneFromTestFile(file_name));
  ASSERT_NE(scene, nullptr);

  std::unique_ptr<Scene> decoded_scene;
  SceneToDecodedGltfScene(*scene, "sphere_two_tex_coords_roundtrip.gltf",
                          &decoded_scene);
  ASSERT_NE(decoded_scene, nullptr);

  ASSERT_EQ(decoded_scene->GetMesh(MeshIndex(0))
                .NumNamedAttributes(GeometryAttribute::TEX_COORD),
            2);
}

TEST_F(GltfEncoderTest, TestSignedAttributesEncoding) {
  auto mesh = std::make_unique<Mesh>();
  // Add 3 positions.
  GeometryAttribute pos_att;
  pos_att.Init(GeometryAttribute::POSITION, nullptr, 3, DT_FLOAT32, false,
               3 * sizeof(float), 0);
  const uint32_t pos_att_id = mesh->AddAttribute(pos_att, true, 3);
  mesh->attribute(pos_att_id)
      ->SetAttributeValue(draco::AttributeValueIndex(0),
                          draco::Vector3f(0, 0, 0).data());
  mesh->attribute(pos_att_id)
      ->SetAttributeValue(draco::AttributeValueIndex(1),
                          draco::Vector3f(1, 0, 0).data());
  mesh->attribute(pos_att_id)
      ->SetAttributeValue(draco::AttributeValueIndex(2),
                          draco::Vector3f(0, 1, 0).data());

  // Helper to add generic attribute
  auto add_gen_att = [&](const std::string& name, draco::DataType dt,
                         const std::vector<int64_t>& values) {
    const int component_size = draco::DataTypeLength(dt);
    GeometryAttribute gen_att;
    gen_att.Init(GeometryAttribute::GENERIC, nullptr, 1, dt, false,
                 component_size, 0);
    const uint32_t att_id = mesh->AddAttribute(gen_att, true, 3);
    for (int i = 0; i < 3; ++i) {
      switch (dt) {
        case DT_INT8: {
          int8_t v = values[i];
          mesh->attribute(att_id)->SetAttributeValue(
              draco::AttributeValueIndex(i), &v);
        } break;
        case DT_INT16: {
          int16_t v = values[i];
          mesh->attribute(att_id)->SetAttributeValue(
              draco::AttributeValueIndex(i), &v);
        } break;
        case DT_INT32: {
          int32_t v = values[i];
          mesh->attribute(att_id)->SetAttributeValue(
              draco::AttributeValueIndex(i), &v);
        } break;
        default:
          FAIL() << "Unsupported type";
      }
    }
    auto metadata = std::make_unique<draco::AttributeMetadata>();
    metadata->AddEntryString("//GLTF/ApplicationSpecificAttributeName", name);
    mesh->AddAttributeMetadata(att_id, std::move(metadata));
  };

  add_gen_att("_INT8", DT_INT8, {-1, 0, 1});
  add_gen_att("_INT16", DT_INT16, {-1000, 0, 1000});

  mesh->SetNumFaces(1);
  mesh->set_num_points(3);
  draco::Mesh::Face face;
  face[0] = draco::PointIndex(0);
  face[1] = draco::PointIndex(1);
  face[2] = draco::PointIndex(2);
  mesh->SetFace(draco::FaceIndex(0), face);

  // Encode mesh to file and decode back
  const std::string gltf_file_full_path =
      draco::GetTestTempFileFullPath("test.gltf");
  std::string folder_path;
  std::string gltf_file_name;
  draco::SplitPath(gltf_file_full_path, &folder_path, &gltf_file_name);
  GltfEncoder gltf_encoder;
  ASSERT_TRUE(
      gltf_encoder.EncodeToFile<Mesh>(*mesh, gltf_file_full_path, folder_path))
      << "Encode failed";

  GltfDecoder decoder;
  auto maybe_mesh = decoder.DecodeFromFile(gltf_file_full_path);
  ASSERT_TRUE(maybe_mesh.ok())
      << "Decode failed: " << maybe_mesh.status().error_msg_string();
  std::unique_ptr<Mesh> decoded_mesh = std::move(maybe_mesh).value();
  ASSERT_NE(decoded_mesh, nullptr);

  // Verify decoded attributes
  auto check_att = [&](const std::string& name, draco::DataType expected_type,
                       const std::vector<int64_t>& expected_values) {
    const int att_id = decoded_mesh->GetAttributeIdByMetadataEntry(
        "//GLTF/ApplicationSpecificAttributeName", name);
    ASSERT_NE(att_id, -1) << "Attribute " << name << " not found";
    const auto* const att = decoded_mesh->attribute(att_id);
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
}

TEST_F(GltfEncoderTest, SimpleTexCoordAttributeUint16) {
  auto mesh = std::make_unique<Mesh>();
  GeometryAttribute pos_att;
  pos_att.Init(GeometryAttribute::POSITION, nullptr, 3, DT_FLOAT32, false,
               3 * sizeof(float), 0);
  const uint32_t pos_att_id = mesh->AddAttribute(pos_att, true, 3);
  mesh->attribute(pos_att_id)
      ->SetAttributeValue(draco::AttributeValueIndex(0),
                          draco::Vector3f(0, 0, 0).data());
  mesh->attribute(pos_att_id)
      ->SetAttributeValue(draco::AttributeValueIndex(1),
                          draco::Vector3f(1, 0, 0).data());
  mesh->attribute(pos_att_id)
      ->SetAttributeValue(draco::AttributeValueIndex(2),
                          draco::Vector3f(0, 1, 0).data());

  GeometryAttribute tex_att;
  tex_att.Init(GeometryAttribute::TEX_COORD, nullptr, 2, DT_UINT16, true,
               2 * sizeof(uint16_t), 0);
  const uint32_t tex_att_id = mesh->AddAttribute(tex_att, true, 3);

  uint16_t uv0[] = {0, 0};
  uint16_t uv1[] = {32767, 65535};
  uint16_t uv2[] = {65535, 32767};
  mesh->attribute(tex_att_id)
      ->SetAttributeValue(draco::AttributeValueIndex(0), uv0);
  mesh->attribute(tex_att_id)
      ->SetAttributeValue(draco::AttributeValueIndex(1), uv1);
  mesh->attribute(tex_att_id)
      ->SetAttributeValue(draco::AttributeValueIndex(2), uv2);

  mesh->SetNumFaces(1);
  mesh->set_num_points(3);
  draco::Mesh::Face face;
  face[0] = draco::PointIndex(0);
  face[1] = draco::PointIndex(1);
  face[2] = draco::PointIndex(2);
  mesh->SetFace(draco::FaceIndex(0), face);

  std::unique_ptr<Mesh> decoded_mesh;
  MeshToDecodedGltfMesh(*mesh, &decoded_mesh);

  ASSERT_EQ(decoded_mesh->NumNamedAttributes(GeometryAttribute::TEX_COORD), 1);
  const auto* decoded_tex_att =
      decoded_mesh->GetNamedAttribute(GeometryAttribute::TEX_COORD);
  ASSERT_NE(decoded_tex_att, nullptr);
  ASSERT_EQ(decoded_tex_att->num_components(), 2);
  ASSERT_EQ(decoded_tex_att->data_type(), DataType::DT_UINT16);
  ASSERT_TRUE(decoded_tex_att->normalized());

  std::array<uint16_t, 2> val0, val1, val2;
  decoded_tex_att->GetValue(draco::AttributeValueIndex(0), &val0);
  decoded_tex_att->GetValue(draco::AttributeValueIndex(1), &val1);
  decoded_tex_att->GetValue(draco::AttributeValueIndex(2), &val2);

  EXPECT_EQ(val0[0], uv0[0]);
  EXPECT_EQ(val0[1], uv0[1]);
  EXPECT_EQ(val1[0], uv1[0]);
  EXPECT_EQ(val1[1], uv1[1]);
  EXPECT_EQ(val2[0], uv2[0]);
  EXPECT_EQ(val2[1], uv2[1]);
}

TEST_F(GltfEncoderTest, SimpleTexCoordAttributeUint8) {
  auto mesh = std::make_unique<Mesh>();
  GeometryAttribute pos_att;
  pos_att.Init(GeometryAttribute::POSITION, nullptr, 3, DT_FLOAT32, false,
               3 * sizeof(float), 0);
  const uint32_t pos_att_id = mesh->AddAttribute(pos_att, true, 3);
  mesh->attribute(pos_att_id)
      ->SetAttributeValue(draco::AttributeValueIndex(0),
                          draco::Vector3f(0, 0, 0).data());
  mesh->attribute(pos_att_id)
      ->SetAttributeValue(draco::AttributeValueIndex(1),
                          draco::Vector3f(1, 0, 0).data());
  mesh->attribute(pos_att_id)
      ->SetAttributeValue(draco::AttributeValueIndex(2),
                          draco::Vector3f(0, 1, 0).data());

  GeometryAttribute tex_att;
  tex_att.Init(GeometryAttribute::TEX_COORD, nullptr, 2, DT_UINT8, true,
               2 * sizeof(uint8_t), 0);
  const uint32_t tex_att_id = mesh->AddAttribute(tex_att, true, 3);

  uint8_t uv0[] = {0, 0};
  uint8_t uv1[] = {127, 255};
  uint8_t uv2[] = {255, 127};
  mesh->attribute(tex_att_id)
      ->SetAttributeValue(draco::AttributeValueIndex(0), uv0);
  mesh->attribute(tex_att_id)
      ->SetAttributeValue(draco::AttributeValueIndex(1), uv1);
  mesh->attribute(tex_att_id)
      ->SetAttributeValue(draco::AttributeValueIndex(2), uv2);

  mesh->SetNumFaces(1);
  mesh->set_num_points(3);
  draco::Mesh::Face face;
  face[0] = draco::PointIndex(0);
  face[1] = draco::PointIndex(1);
  face[2] = draco::PointIndex(2);
  mesh->SetFace(draco::FaceIndex(0), face);

  std::unique_ptr<Mesh> decoded_mesh;
  MeshToDecodedGltfMesh(*mesh, &decoded_mesh);

  ASSERT_EQ(decoded_mesh->NumNamedAttributes(GeometryAttribute::TEX_COORD), 1);
  const auto* decoded_tex_att =
      decoded_mesh->GetNamedAttribute(GeometryAttribute::TEX_COORD);
  ASSERT_NE(decoded_tex_att, nullptr);
  ASSERT_EQ(decoded_tex_att->num_components(), 2);
  ASSERT_EQ(decoded_tex_att->data_type(), DataType::DT_UINT8);
  ASSERT_TRUE(decoded_tex_att->normalized());

  std::array<uint8_t, 2> val0, val1, val2;
  decoded_tex_att->GetValue(draco::AttributeValueIndex(0), &val0);
  decoded_tex_att->GetValue(draco::AttributeValueIndex(1), &val1);
  decoded_tex_att->GetValue(draco::AttributeValueIndex(2), &val2);

  EXPECT_EQ(val0[0], uv0[0]);
  EXPECT_EQ(val0[1], uv0[1]);
  EXPECT_EQ(val1[0], uv1[0]);
  EXPECT_EQ(val1[1], uv1[1]);
  EXPECT_EQ(val2[0], uv2[0]);
  EXPECT_EQ(val2[1], uv2[1]);
}

TEST_F(GltfEncoderTest, IndicesBufferViewTightlyPacked) {
  // Verify that for an odd number of faces with uint16 indices (max_index >=
  // 255), the indices bufferView byteLength is tightly packed (6 bytes for 1
  // face) rather than padded to 8 bytes, while the next bufferView starts at
  // byteOffset 8.
  auto mesh = std::make_unique<Mesh>();
  const int num_points = 300;
  GeometryAttribute pos_att;
  pos_att.Init(GeometryAttribute::POSITION, nullptr, 3, DT_FLOAT32, false,
               3 * sizeof(float), 0);
  const uint32_t pos_att_id = mesh->AddAttribute(pos_att, true, num_points);
  for (int i = 0; i < num_points; ++i) {
    mesh->attribute(pos_att_id)
        ->SetAttributeValue(draco::AttributeValueIndex(i),
                            draco::Vector3f(i, 0, 0).data());
  }

  mesh->SetNumFaces(1);
  mesh->set_num_points(num_points);
  draco::Mesh::Face face;
  face[0] = draco::PointIndex(0);
  face[1] = draco::PointIndex(1);
  face[2] = draco::PointIndex(num_points - 1);
  mesh->SetFace(draco::FaceIndex(0), face);

  const std::string gltf_file_full_path =
      draco::GetTestTempFileFullPath("indices_tightly_packed.gltf");
  EncodeMeshToFile(*mesh, gltf_file_full_path);

  CheckGltfFileAtLeastStringCount(gltf_file_full_path, "\"byteLength\": 6", 1);
  CheckGltfFileAtLeastStringCount(gltf_file_full_path, "\"byteOffset\": 8", 1);
}

}  // namespace draco

#endif  // DRACO_TRANSCODER_SUPPORTED
