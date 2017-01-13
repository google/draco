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
#include "compression/point_cloud/point_cloud_kd_tree_decoder.h"
#include "compression/point_cloud/point_cloud_kd_tree_encoder.h"
#include "core/draco_test_base.h"
#include "core/draco_test_utils.h"
#include "core/vector_d.h"
#include "io/obj_decoder.h"
#include "point_cloud/point_cloud_builder.h"

namespace draco {

class PointCloudKdTreeEncodingTest : public ::testing::Test {
 protected:
  std::unique_ptr<PointCloud> DecodeObj(const std::string &file_name) const {
    const std::string path = GetTestFileFullPath(file_name);
    ObjDecoder decoder;
    std::unique_ptr<PointCloud> pc(new PointCloud());
    if (!decoder.DecodeFromFile(path, pc.get()))
      return nullptr;
    return pc;
  }

  void ComparePointClouds(const PointCloud &p0, const PointCloud &p1) const {
    ASSERT_EQ(p0.num_points(), p1.num_points());
    ASSERT_EQ(p0.num_attributes(), p1.num_attributes());
    // Currently works only with one attribute.
    ASSERT_EQ(p0.num_attributes(), 1);
    ASSERT_EQ(p0.attribute(0)->components_count(), 3);
    std::vector<VectorD<double, 3>> points0, points1;
    for (PointIndex i(0); i < p0.num_points(); ++i) {
      VectorD<double, 3> pos0, pos1;
      p0.attribute(0)->ConvertValue(p0.attribute(0)->mapped_index(i), &pos0[0]);
      p1.attribute(0)->ConvertValue(p1.attribute(0)->mapped_index(i), &pos1[0]);
      points0.push_back(pos0);
      points1.push_back(pos1);
    }
    // To compare the point clouds we sort points from both inputs separately,
    // and then we compare all matching points one by one.
    // TODO(ostava): Note that this is not guaranteed to work for quantized
    // point clouds because the order of points may actually change because
    // of the quantization. The test should be make more robust to handle such
    // case.
    std::sort(points0.begin(), points0.end());
    std::sort(points1.begin(), points1.end());
    for (uint32_t i = 0; i < points0.size(); ++i) {
      ASSERT_LE((points0[i] - points1[i]).SquaredNorm(), 1e-2);
    }
  }

  void TestKdTreeEncoding(const PointCloud &pc) {
    EncoderBuffer buffer;
    PointCloudKdTreeEncoder encoder;
    EncoderOptions options = EncoderOptions::CreateDefaultOptions();
    options.SetGlobalInt("quantization_bits", 12);
    encoder.SetPointCloud(pc);
    ASSERT_TRUE(encoder.Encode(options, &buffer));

    DecoderBuffer dec_buffer;
    dec_buffer.Init(buffer.data(), buffer.size());
    PointCloudKdTreeDecoder decoder;

    std::unique_ptr<PointCloud> out_pc(new PointCloud());
    ASSERT_TRUE(decoder.Decode(&dec_buffer, out_pc.get()));

    ComparePointClouds(pc, *out_pc.get());
  }

  void TestFloatEncoding(const std::string &file_name) {
    std::unique_ptr<PointCloud> pc = DecodeObj(file_name);
    ASSERT_NE(pc, nullptr);

    TestKdTreeEncoding(*pc.get());
  }
};

TEST_F(PointCloudKdTreeEncodingTest, TestFloatKdTreeEncoding) {
  TestFloatEncoding("cube_subd.obj");
}

TEST_F(PointCloudKdTreeEncodingTest, TestIntKdTreeEncoding) {
  constexpr int num_points = 120;
  std::vector<std::array<uint32_t, 3>> points(num_points);
  for (int i = 0; i < num_points; ++i) {
    std::array<uint32_t, 3> pos;
    // Generate some pseudo-random points.
    pos[0] = 8 * ((i * 7) % 127);
    pos[1] = 13 * ((i * 3) % 321);
    pos[2] = 29 * ((i * 19) % 450);
    points[i] = pos;
  }

  PointCloudBuilder builder;
  builder.Start(num_points);
  const int att_id =
      builder.AddAttribute(GeometryAttribute::POSITION, 3, DT_UINT32);
  for (PointIndex i(0); i < num_points; ++i) {
    builder.SetAttributeValueForPoint(att_id, PointIndex(i),
                                      &(points[i.value()])[0]);
  }
  std::unique_ptr<PointCloud> pc = builder.Finalize(false);
  ASSERT_NE(pc, nullptr);

  TestKdTreeEncoding(*pc.get());
}

}  // namespace draco
