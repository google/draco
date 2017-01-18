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
#include <sstream>

#include "compression/mesh/mesh_edgebreaker_decoder.h"
#include "compression/mesh/mesh_edgebreaker_encoder.h"
#include "core/draco_test_base.h"
#include "core/draco_test_utils.h"
#include "io/mesh_io.h"
#include "io/obj_decoder.h"
#include "mesh/mesh_are_equivalent.h"
#include "mesh/mesh_cleanup.h"
#include "mesh/triangle_soup_mesh_builder.h"

namespace draco {

class MeshEdgebreakerEncodingTest : public ::testing::Test {
 protected:
  void TestFile(const std::string &file_name) {
    TestFile(file_name, -1);
  }

  void TestFile(const std::string &file_name, int compression_level) {
    const std::string path = GetTestFileFullPath(file_name);
    const std::unique_ptr<Mesh> mesh(ReadMeshFromFile(path));
    ASSERT_NE(mesh, nullptr) << "Failed to load test model " << file_name;

    TestMesh(mesh.get(), compression_level);
  }

  void TestMesh(Mesh *mesh, int compression_level) {
    EncoderBuffer buffer;
    MeshEdgeBreakerEncoder encoder;
    EncoderOptions encoder_options = CreateDefaultEncoderOptions();
    if (compression_level != -1) {
      SetSpeedOptions(&encoder_options, 10 - compression_level,
                      10 - compression_level);
    }
    encoder.SetMesh(*mesh);
    ASSERT_TRUE(encoder.Encode(encoder_options, &buffer));

    DecoderBuffer dec_buffer;
    dec_buffer.Init(buffer.data(), buffer.size());
    MeshEdgeBreakerDecoder decoder;

    std::unique_ptr<Mesh> decoded_mesh(new Mesh());
    ASSERT_TRUE(decoder.Decode(&dec_buffer, decoded_mesh.get()));

    // Cleanup the input mesh to make sure that input and output can be
    // compared (edgebreaker method discards degenerated triangles and isolated
    // vertices).
    const MeshCleanupOptions options;
    MeshCleanup cleanup;
    ASSERT_TRUE(cleanup(mesh, options)) << "Failed to clean the input mesh.";

    MeshAreEquivalent eq;
    ASSERT_TRUE(eq(*mesh, *decoded_mesh.get()))
        << "Decoded mesh is not the same as the input";
  }
};

TEST_F(MeshEdgebreakerEncodingTest, TestNmOBJ) {
  const std::string file_name = "test_nm.obj";
  TestFile(file_name);
}

TEST_F(MeshEdgebreakerEncodingTest, ThreeFacesOBJ) {
  const std::string file_name = "extra_vertex.obj";
  TestFile(file_name);
}

TEST_F(MeshEdgebreakerEncodingTest, TestPly) {
  // Tests whether the edgebreaker successfully encodes and decodes the test
  // file (ply with color).
  const std::string file_name = "test_pos_color.ply";
  TestFile(file_name);
}

TEST_F(MeshEdgebreakerEncodingTest, TestMultiAttributes) {
  // Tests encoding of model with many attributes.
  const std::string file_name = "cube_att.obj";
  TestFile(file_name, 10);
}

TEST_F(MeshEdgebreakerEncodingTest, TestEncoderReuse) {
  // Tests whether the edgebreaker encoder can be reused multiple times to
  // encode a given mesh.
  const std::string file_name = "test_pos_color.ply";
  const std::string path = GetTestFileFullPath(file_name);
  const std::unique_ptr<Mesh> mesh(ReadMeshFromFile(path));
  ASSERT_NE(mesh, nullptr) << "Failed to load test model " << file_name;

  MeshEdgeBreakerEncoder encoder;
  EncoderOptions encoder_options = CreateDefaultEncoderOptions();
  encoder.SetMesh(*mesh);
  EncoderBuffer buffer_0, buffer_1;
  ASSERT_TRUE(encoder.Encode(encoder_options, &buffer_0));
  ASSERT_TRUE(encoder.Encode(encoder_options, &buffer_1));

  // Make sure both buffer are identical.
  ASSERT_EQ(buffer_0.size(), buffer_1.size());
  for (int i = 0; i < buffer_0.size(); ++i) {
    ASSERT_EQ(buffer_0.data()[i], buffer_1.data()[i]);
  }
}

TEST_F(MeshEdgebreakerEncodingTest, TestDecoderReuse) {
  // Tests whether the edgebreaker decoder can be reused multiple times to
  // decode a given mesh.
  const std::string file_name = "test_pos_color.ply";
  const std::string path = GetTestFileFullPath(file_name);
  const std::unique_ptr<Mesh> mesh(ReadMeshFromFile(path));
  ASSERT_NE(mesh, nullptr) << "Failed to load test model " << file_name;

  MeshEdgeBreakerEncoder encoder;
  EncoderOptions encoder_options = CreateDefaultEncoderOptions();
  encoder.SetMesh(*mesh);
  EncoderBuffer buffer;
  ASSERT_TRUE(encoder.Encode(encoder_options, &buffer));

  DecoderBuffer dec_buffer;
  dec_buffer.Init(buffer.data(), buffer.size());

  MeshEdgeBreakerDecoder decoder;

  // Decode the mesh two times.
  std::unique_ptr<Mesh> decoded_mesh_0(new Mesh());
  ASSERT_TRUE(decoder.Decode(&dec_buffer, decoded_mesh_0.get()));

  dec_buffer.Init(buffer.data(), buffer.size());
  std::unique_ptr<Mesh> decoded_mesh_1(new Mesh());
  ASSERT_TRUE(decoder.Decode(&dec_buffer, decoded_mesh_1.get()));

  // Make sure both of the meshes are identical.
  MeshAreEquivalent eq;
  ASSERT_TRUE(eq(*decoded_mesh_0.get(), *decoded_mesh_1.get()))
      << "Decoded meshes are not the same";
}

}  // namespace draco
