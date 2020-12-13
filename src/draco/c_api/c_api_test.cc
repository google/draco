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

#include "draco/draco_features.h"

#ifdef DRACO_C_API_SUPPORTED

#include <fstream>
#include <memory>
#include <sstream>
#include <string>

#include "draco/c_api/c_api.h"
#include "draco/core/draco_test_base.h"
#include "draco/core/draco_test_utils.h"

namespace {

draco_mesh *DecodeToDracoMesh(const std::string &file_name) {
  std::ifstream input_file(draco::GetTestFileFullPath(file_name),
                           std::ios::binary);
  if (!input_file) {
    return nullptr;
  }
  // Read the file stream into a buffer.
  std::streampos file_size = 0;
  input_file.seekg(0, std::ios::end);
  file_size = input_file.tellg() - file_size;
  input_file.seekg(0, std::ios::beg);
  std::vector<char> data(file_size);
  input_file.read(data.data(), file_size);
  if (data.empty()) {
    return nullptr;
  }
  
  auto mesh = dracoNewMesh();
  auto decoder = dracoNewDecoder();
  dracoDecoderArrayToMesh(decoder, data.data(), data.size(), mesh);
  dracoDecoderRelease(decoder);
  return mesh;
}

TEST(DracoCAPITest, TestDecode) {
  auto mesh = DecodeToDracoMesh("test_nm.obj.edgebreaker.cl4.2.2.drc");
  ASSERT_NE(mesh, nullptr);
  auto num_faces = dracoMeshNumFaces(mesh);
  ASSERT_EQ(num_faces, 170);
  ASSERT_EQ(dracoPointCloudNumPoints(mesh), 99);

  auto indices_size = 3 * num_faces * sizeof(uint32_t);
  uint32_t *indices = (uint32_t *)malloc(indices_size);
  ASSERT_TRUE(dracoMeshGetTrianglesUint32(mesh, indices_size, indices));
  free(indices);
  dracoMeshRelease(mesh);
}

}  // namespace

#endif // DRACO_C_API_SUPPORTED