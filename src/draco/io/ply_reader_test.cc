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
#include "draco/io/ply_reader.h"

#include "draco/core/draco_test_base.h"
#include "draco/core/draco_test_utils.h"
#include "draco/io/file_utils.h"
#include "draco/io/ply_property_reader.h"

namespace draco {

class PlyReaderTest : public ::testing::Test {
 protected:
  std::vector<char> ReadPlyFile(const std::string &file_name) const {
    const std::string path = GetTestFileFullPath(file_name);

    std::vector<char> data;
    EXPECT_TRUE(ReadFileToBuffer(path, &data));
    return data;
  }
};

TEST_F(PlyReaderTest, TestReader) {
  const std::string file_name = "test_pos_color.ply";
  const std::vector<char> data = ReadPlyFile(file_name);
  DecoderBuffer buf;
  buf.Init(data.data(), data.size());
  PlyReader reader;
  Status status = reader.Read(&buf);
  DRACO_ASSERT_OK(status);
  ASSERT_EQ(reader.num_elements(), 2);
  ASSERT_EQ(reader.element(0).num_properties(), 7);
  ASSERT_EQ(reader.element(1).num_properties(), 1);
  ASSERT_TRUE(reader.element(1).property(0).is_list());

  ASSERT_TRUE(reader.element(0).GetPropertyByName("red") != nullptr);
  const PlyProperty *const prop = reader.element(0).GetPropertyByName("red");
  PlyPropertyReader<uint8_t> reader_uint8(prop);
  PlyPropertyReader<uint32_t> reader_uint32(prop);
  PlyPropertyReader<float> reader_float(prop);
  for (int i = 0; i < reader.element(0).num_entries(); ++i) {
    ASSERT_EQ(reader_uint8.ReadValue(i), reader_uint32.ReadValue(i));
    ASSERT_EQ(reader_uint8.ReadValue(i), reader_float.ReadValue(i));
  }
}

TEST_F(PlyReaderTest, TestReaderAscii) {
  const std::string file_name = "test_pos_color.ply";
  const std::vector<char> data = ReadPlyFile(file_name);
  ASSERT_NE(data.size(), 0u);
  DecoderBuffer buf;
  buf.Init(data.data(), data.size());
  PlyReader reader;
  Status status = reader.Read(&buf);
  DRACO_ASSERT_OK(status);

  const std::string file_name_ascii = "test_pos_color_ascii.ply";
  const std::vector<char> data_ascii = ReadPlyFile(file_name_ascii);
  buf.Init(data_ascii.data(), data_ascii.size());
  PlyReader reader_ascii;
  status = reader_ascii.Read(&buf);
  DRACO_ASSERT_OK(status);
  ASSERT_EQ(reader.num_elements(), reader_ascii.num_elements());
  ASSERT_EQ(reader.element(0).num_properties(),
            reader_ascii.element(0).num_properties());

  ASSERT_TRUE(reader.element(0).GetPropertyByName("x") != nullptr);
  const PlyProperty *const prop = reader.element(0).GetPropertyByName("x");
  const PlyProperty *const prop_ascii =
      reader_ascii.element(0).GetPropertyByName("x");
  PlyPropertyReader<float> reader_float(prop);
  PlyPropertyReader<float> reader_float_ascii(prop_ascii);
  for (int i = 0; i < reader.element(0).num_entries(); ++i) {
    ASSERT_NEAR(reader_float.ReadValue(i), reader_float_ascii.ReadValue(i),
                1e-4f);
  }
}

TEST_F(PlyReaderTest, TestReaderExtraWhitespace) {
  const std::string file_name = "test_extra_whitespace.ply";
  const std::vector<char> data = ReadPlyFile(file_name);
  ASSERT_NE(data.size(), 0u);
  DecoderBuffer buf;
  buf.Init(data.data(), data.size());
  PlyReader reader;
  Status status = reader.Read(&buf);
  DRACO_ASSERT_OK(status);

  ASSERT_EQ(reader.num_elements(), 2);
  ASSERT_EQ(reader.element(0).num_properties(), 7);
  ASSERT_EQ(reader.element(1).num_properties(), 1);
  ASSERT_TRUE(reader.element(1).property(0).is_list());

  ASSERT_TRUE(reader.element(0).GetPropertyByName("red") != nullptr);
  const PlyProperty *const prop = reader.element(0).GetPropertyByName("red");
  PlyPropertyReader<uint8_t> reader_uint8(prop);
  PlyPropertyReader<uint32_t> reader_uint32(prop);
  PlyPropertyReader<float> reader_float(prop);
  for (int i = 0; i < reader.element(0).num_entries(); ++i) {
    ASSERT_EQ(reader_uint8.ReadValue(i), reader_uint32.ReadValue(i));
    ASSERT_EQ(reader_uint8.ReadValue(i), reader_float.ReadValue(i));
  }
}

TEST_F(PlyReaderTest, TestReaderMoreDataTypes) {
  const std::string file_name = "test_more_datatypes.ply";
  const std::vector<char> data = ReadPlyFile(file_name);
  ASSERT_NE(data.size(), 0u);
  DecoderBuffer buf;
  buf.Init(data.data(), data.size());
  PlyReader reader;
  Status status = reader.Read(&buf);
  DRACO_ASSERT_OK(status);

  ASSERT_EQ(reader.num_elements(), 2);
  ASSERT_EQ(reader.element(0).num_properties(), 7);
  ASSERT_EQ(reader.element(1).num_properties(), 1);
  ASSERT_TRUE(reader.element(1).property(0).is_list());

  ASSERT_TRUE(reader.element(0).GetPropertyByName("red") != nullptr);
  const PlyProperty *const prop = reader.element(0).GetPropertyByName("red");
  PlyPropertyReader<uint8_t> reader_uint8(prop);
  PlyPropertyReader<uint32_t> reader_uint32(prop);
  PlyPropertyReader<float> reader_float(prop);
  for (int i = 0; i < reader.element(0).num_entries(); ++i) {
    ASSERT_EQ(reader_uint8.ReadValue(i), reader_uint32.ReadValue(i));
    ASSERT_EQ(reader_uint8.ReadValue(i), reader_float.ReadValue(i));
  }
}

TEST_F(PlyReaderTest, TestReaderTruncatedListData) {
  // Binary PLY where the "face" element declares a list property whose
  // count field claims far more entries than the remaining buffer can hold.
  // Regression test for a heap-buffer-overflow read in
  // PlyReader::ParseElementData(): the list count was previously used to
  // copy data out of the input buffer without a bounds check.
  const char kData[] =
      "ply\n"
      "format binary_little_endian 1.0\n"
      "element vertex 1\n"
      "property float x\n"
      "property float y\n"
      "property float z\n"
      "element face 1\n"
      "property list uchar int vertex_indices\n"
      "end_header\n"
      "\x00\x00\x80\x3f\x00\x00\x00\x40\x00\x00\x40\x40"  // vertex: 1, 2, 3
      "\xff"      // list count claims 255 int32 entries (1020 bytes)
      "\x41\x41"  // but only 2 bytes of data actually follow
      ;
  DecoderBuffer buf;
  buf.Init(kData, sizeof(kData) - 1);
  PlyReader reader;
  const Status status = reader.Read(&buf);
  ASSERT_FALSE(status.ok());
}

}  // namespace draco
