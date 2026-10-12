// Copyright 2022 The Draco Authors.
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
#include "draco/io/stl_decoder.h"

#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "draco/core/draco_test_base.h"
#include "draco/core/draco_test_utils.h"

namespace draco {

class StlDecoderTest : public ::testing::Test {
 protected:
  void test_decoding(const std::string& file_name) {
    const std::string path = GetTestFileFullPath(file_name);
    StlDecoder decoder;
    DRACO_ASSIGN_OR_ASSERT(std::unique_ptr<Mesh> mesh,
                           decoder.DecodeFromFile(path));
    ASSERT_GT(mesh->num_faces(), 0);
    ASSERT_GT(mesh->num_points(), 0);
  }

  void test_decoding_should_fail(const std::string& file_name) {
    StlDecoder decoder;
    StatusOr<std::unique_ptr<Mesh>> statusOrMesh =
        decoder.DecodeFromFile(GetTestFileFullPath(file_name));
    ASSERT_FALSE(statusOrMesh.ok());
  }
};

TEST_F(StlDecoderTest, TestStlDecoding) {
  test_decoding("STL/bunny.stl");
  test_decoding("STL/test_sphere.stl");
  test_decoding_should_fail("STL/test_sphere_ascii.stl");
}

TEST_F(StlDecoderTest, TestEmptyFile) {
  test_decoding_should_fail("STL/empty.stl");
}

TEST_F(StlDecoderTest, TestTruncatedHeader) {
  std::vector<char> data(50, 0);
  DecoderBuffer buffer;
  buffer.Init(data.data(), data.size());
  StlDecoder decoder;
  auto status_or_mesh = decoder.DecodeFromBuffer(&buffer);
  EXPECT_FALSE(status_or_mesh.ok());
}

TEST_F(StlDecoderTest, TestFaceCountExceedsBuffer) {
  // 80 bytes header + 4 bytes face count claiming 100 faces (needs 5000 bytes).
  std::vector<char> data(84, 0);
  uint32_t face_count = 100;
  memcpy(data.data() + 80, &face_count, 4);
  DecoderBuffer buffer;
  buffer.Init(data.data(), data.size());
  StlDecoder decoder;
  auto status_or_mesh = decoder.DecodeFromBuffer(&buffer);
  EXPECT_FALSE(status_or_mesh.ok());
}

TEST_F(StlDecoderTest, TestFaceCountMultiplicationOverflow) {
  // 80 bytes header + 4 bytes face count where face_count * 50 overflows
  // uint32.
  std::vector<char> data(84, 0);
  uint32_t face_count = 0x10000000;
  memcpy(data.data() + 80, &face_count, 4);
  DecoderBuffer buffer;
  buffer.Init(data.data(), data.size());
  StlDecoder decoder;
  auto status_or_mesh = decoder.DecodeFromBuffer(&buffer);
  EXPECT_FALSE(status_or_mesh.ok());
}

}  // namespace draco
