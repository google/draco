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
#include "draco/compression/decode.h"

#include <cinttypes>
#include <fstream>
#include <sstream>

#include "draco/core/draco_test_base.h"
#include "draco/core/draco_test_utils.h"

namespace {

class DecodeTest : public ::testing::Test {
 protected:
  DecodeTest() {}
};

TEST_F(DecodeTest, TestSkipAttributeTransform) {
  const std::string file_name = "test_nm_quant.0.9.0.drc";
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

  // Create a draco decoding buffer. Note that no data is copied in this step.
  draco::DecoderBuffer buffer;
  buffer.Init(data.data(), data.size());

  draco::Decoder decoder;
  // Make sure we skip dequantization for the position attribute.
  decoder.SetSkipAttributeTransform(draco::GeometryAttribute::POSITION);

  // Decode the input data into a geometry.
  std::unique_ptr<draco::PointCloud> pc =
      decoder.DecodePointCloudFromBuffer(&buffer).value();
  ASSERT_NE(pc, nullptr);

  const draco::PointAttribute *const pos_att =
      pc->GetNamedAttribute(draco::GeometryAttribute::POSITION);
  ASSERT_NE(pos_att, nullptr);

  // Ensure the position attribute is of type int32_t and that it has a valid
  // attribute transform.
  ASSERT_EQ(pos_att->data_type(), draco::DT_INT32);
  ASSERT_NE(pos_att->GetAttributeTransformData(), nullptr);

  // Normal attribute should be left transformed.
  const draco::PointAttribute *const norm_att =
      pc->GetNamedAttribute(draco::GeometryAttribute::NORMAL);
  ASSERT_EQ(norm_att->data_type(), draco::DT_FLOAT32);
  ASSERT_EQ(norm_att->GetAttributeTransformData(), nullptr);
}

}  // namespace
