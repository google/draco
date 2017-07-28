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

#include <cinttypes>
#include <fstream>
#include <sstream>

#include "draco/attributes/attribute_quantization_transform.h"
#include "draco/compression/decode.h"
#include "draco/compression/encode.h"
#include "draco/compression/expert_encode.h"
#include "draco/core/draco_test_base.h"
#include "draco/core/draco_test_utils.h"
#include "draco/core/vector_d.h"
#include "draco/mesh/triangle_soup_mesh_builder.h"

namespace {

class EncodeTest : public ::testing::Test {
 protected:
  EncodeTest() {}
  std::unique_ptr<draco::Mesh> CreateTestMesh() const {
    draco::TriangleSoupMeshBuilder mesh_builder;

    // Create a simple mesh with one face.
    mesh_builder.Start(1);

    // Add one position attribute and two texture coordinate attributes.
    const int32_t pos_att_id = mesh_builder.AddAttribute(
        draco::GeometryAttribute::POSITION, 3, draco::DT_FLOAT32);
    const int32_t tex_att_id_0 = mesh_builder.AddAttribute(
        draco::GeometryAttribute::TEX_COORD, 2, draco::DT_FLOAT32);
    const int32_t tex_att_id_1 = mesh_builder.AddAttribute(
        draco::GeometryAttribute::TEX_COORD, 2, draco::DT_FLOAT32);

    // Initialize the attribute values.
    mesh_builder.SetAttributeValuesForFace(
        pos_att_id, draco::FaceIndex(0), draco::Vector3f(0.f, 0.f, 0.f).data(),
        draco::Vector3f(1.f, 0.f, 0.f).data(),
        draco::Vector3f(1.f, 1.f, 0.f).data());
    mesh_builder.SetAttributeValuesForFace(
        tex_att_id_0, draco::FaceIndex(0), draco::Vector2f(0.f, 0.f).data(),
        draco::Vector2f(1.f, 0.f).data(), draco::Vector2f(1.f, 1.f).data());
    mesh_builder.SetAttributeValuesForFace(
        tex_att_id_1, draco::FaceIndex(0), draco::Vector2f(0.f, 0.f).data(),
        draco::Vector2f(1.f, 0.f).data(), draco::Vector2f(1.f, 1.f).data());

    return mesh_builder.Finalize();
  }

  int GetQuantizationBitsFromAttribute(const draco::PointAttribute *att) const {
    if (att == nullptr)
      return -1;
    draco::AttributeQuantizationTransform transform;
    if (!transform.InitFromAttribute(*att))
      return -1;
    return transform.quantization_bits();
  }

  void VerifyNumQuantizationBits(const draco::EncoderBuffer &buffer,
                                 int pos_quantization,
                                 int tex_coord_0_quantization,
                                 int tex_coord_1_quantization) const {
    draco::Decoder decoder;

    // Skip the dequantization for the attributes which will allow us to get
    // the number of quantization bits used during encoding.
    decoder.SetSkipAttributeTransform(draco::GeometryAttribute::POSITION);
    decoder.SetSkipAttributeTransform(draco::GeometryAttribute::TEX_COORD);

    draco::DecoderBuffer in_buffer;
    in_buffer.Init(buffer.data(), buffer.size());
    auto mesh = decoder.DecodeMeshFromBuffer(&in_buffer).value();
    ASSERT_NE(mesh, nullptr);
    ASSERT_EQ(GetQuantizationBitsFromAttribute(mesh->attribute(0)),
              pos_quantization);
    ASSERT_EQ(GetQuantizationBitsFromAttribute(mesh->attribute(1)),
              tex_coord_0_quantization);
    ASSERT_EQ(GetQuantizationBitsFromAttribute(mesh->attribute(2)),
              tex_coord_1_quantization);
  }
};

TEST_F(EncodeTest, TestExpertEncoderQuantization) {
  // This test verifies that the expert encoder can quantize individual
  // attributes even if they have the same type.
  auto mesh = CreateTestMesh();
  ASSERT_NE(mesh, nullptr);

  draco::ExpertEncoder encoder(*mesh.get());
  encoder.SetAttributeQuantization(0, 16);  // Position quantization.
  encoder.SetAttributeQuantization(1, 15);  // Tex-coord 0 quantization.
  encoder.SetAttributeQuantization(2, 14);  // Tex-coord 1 quantization.

  draco::EncoderBuffer buffer;
  encoder.EncodeToBuffer(&buffer);
  VerifyNumQuantizationBits(buffer, 16, 15, 14);
}

TEST_F(EncodeTest, TestEncoderQuantization) {
  // This test verifies that Encoder applies the same quantization to all
  // attributes of the same type.
  auto mesh = CreateTestMesh();
  ASSERT_NE(mesh, nullptr);

  draco::Encoder encoder;
  encoder.SetAttributeQuantization(draco::GeometryAttribute::POSITION, 16);
  encoder.SetAttributeQuantization(draco::GeometryAttribute::TEX_COORD, 15);

  draco::EncoderBuffer buffer;
  encoder.EncodeMeshToBuffer(*mesh.get(), &buffer);
  VerifyNumQuantizationBits(buffer, 16, 15, 15);
}

}  // namespace
