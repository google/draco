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
#include "io/point_cloud_io.h"

#include <sstream>

#include "core/draco_test_base.h"
#include "core/draco_test_utils.h"
#include "io/obj_decoder.h"

namespace draco {

class IoPointCloudIoTest : public ::testing::Test {
 protected:
  std::unique_ptr<PointCloud> DecodeObj(const std::string &file_name) const {
    const std::string path = GetTestFileFullPath(file_name);
    std::unique_ptr<PointCloud> pc(new PointCloud());
    ObjDecoder decoder;
    if (!decoder.DecodeFromFile(path, pc.get()))
      return nullptr;
    return pc;
  }

  void test_compression_method(PointCloudEncodingMethod method,
                               const std::string &file_name) {
    const std::unique_ptr<PointCloud> pc(DecodeObj(file_name));
    ASSERT_NE(pc, nullptr) << "Failed to load test model " << file_name;

    std::stringstream ss;
    WritePointCloudIntoStream(pc.get(), ss, method);
    ASSERT_TRUE(ss.good());

    std::unique_ptr<PointCloud> decoded_pc;
    ReadPointCloudFromStream(&decoded_pc, ss);
    ASSERT_TRUE(ss.good());
  }
};

TEST_F(IoPointCloudIoTest, EncodeWithBinary) {
  test_compression_method(POINT_CLOUD_SEQUENTIAL_ENCODING, "test_nm.obj");
  test_compression_method(POINT_CLOUD_SEQUENTIAL_ENCODING, "sphere.obj");
}

TEST_F(IoPointCloudIoTest, ObjFileInput) {
  // Tests whether loading obj point clouds from files works as expected.
  const std::unique_ptr<PointCloud> pc =
      ReadPointCloudFromFile(GetTestFileFullPath("test_nm.obj"));
  ASSERT_NE(pc, nullptr) << "Failed to load the obj point cloud.";
  EXPECT_EQ(pc->num_points(), 97u) << "Obj point cloud not loaded properly.";
}

}  // namespace draco
