// Copyright 2017 The Draco Authors.
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
#include "draco/unity/draco_unity_plugin.h"

#include <fstream>
#include <memory>
#include <sstream>
#include <string>

#include "draco/core/draco_test_base.h"
#include "draco/core/draco_test_utils.h"

namespace {

class DracoUnityPluginTest : public ::testing::Test {
 protected:
  DracoUnityPluginTest() : unity_mesh_(nullptr) {}

  void TestDecodingToDracoUnityMesh(const std::string &file_name,
                                    int expected_num_faces,
                                    int expected_num_vertices) {
    // Tests that decoders can successfully skip attribute transform.
    std::ifstream input_file(draco::GetTestFileFullPath(file_name),
                             std::ios::binary);
    ASSERT_TRUE(input_file);

    // Read the file stream into a buffer.
    std::streampos file_size = 0;
    input_file.seekg(0, std::ios::end);
    file_size = input_file.tellg() - file_size;
    input_file.seekg(0, std::ios::beg);
    std::vector<char> data(file_size);
    input_file.read(data.data(), file_size);

    ASSERT_FALSE(data.empty());

    const int num_faces =
        draco::DecodeMeshForUnity(data.data(), data.size(), &unity_mesh_);

    ASSERT_EQ(num_faces, expected_num_faces);
    ASSERT_EQ(unity_mesh_->num_faces, expected_num_faces);
    ASSERT_EQ(unity_mesh_->num_vertices, expected_num_vertices);
    ASSERT_TRUE(unity_mesh_->has_normal);
    ASSERT_NE(unity_mesh_->normal, nullptr);
    // TODO(fgalligan): Also test color and tex_coord attributes.

    draco::ReleaseUnityMesh(&unity_mesh_);
  }

  draco::DracoToUnityMesh *unity_mesh_;
};

TEST_F(DracoUnityPluginTest, TestDecodingToDracoUnityMesh) {
  TestDecodingToDracoUnityMesh("test_nm.obj.edgebreaker.1.0.0.drc", 170, 99);
}
}  // namespace
