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
#include "compression/point_cloud/point_cloud_sequential_decoder.h"
#include "compression/point_cloud/point_cloud_sequential_encoder.h"
#include "core/draco_test_base.h"
#include "core/draco_test_utils.h"
#include "io/obj_decoder.h"

namespace draco {

class PointCloudSequentialEncodingTest : public ::testing::Test {
 protected:
  std::unique_ptr<PointCloud> DecodeObj(const std::string &file_name) const {
    const std::string path = GetTestFileFullPath(file_name);
    ObjDecoder decoder;
    std::unique_ptr<PointCloud> pc(new PointCloud());
    if (!decoder.DecodeFromFile(path, pc.get()))
      return nullptr;
    return pc;
  }

  void TestEncoding(const std::string &file_name) {
    std::unique_ptr<PointCloud> pc = DecodeObj(file_name);
    ASSERT_NE(pc, nullptr);

    EncoderBuffer buffer;
    PointCloudSequentialEncoder encoder;
    EncoderOptions options = EncoderOptions::CreateDefaultOptions();
    encoder.SetPointCloud(*pc.get());
    ASSERT_TRUE(encoder.Encode(options, &buffer));

    DecoderBuffer dec_buffer;
    dec_buffer.Init(buffer.data(), buffer.size());
    PointCloudSequentialDecoder decoder;

    std::unique_ptr<PointCloud> out_pc(new PointCloud());
    ASSERT_TRUE(decoder.Decode(&dec_buffer, out_pc.get()));

    ASSERT_EQ(out_pc->num_points(), pc->num_points());
  }
};

TEST_F(PointCloudSequentialEncodingTest, DoesEncodeAndDecode) {
  TestEncoding("test_nm.obj");
}

// TODO(ostava): Test the reusability of a single instance of the encoder and
// decoder class.

}  // namespace draco
