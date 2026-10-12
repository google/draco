// Copyright 2026 The Draco Authors.
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
#ifdef DRACO_PMC_SUPPORTED

#include "draco/compression/pmc/pmc_utils.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <pmc/pmCodec.hpp>
#include <pmc/pmTypesAndConstants.hpp>
#include <string>
#include <vector>

#include "draco/attributes/geometry_attribute.h"
#include "draco/core/bounding_box.h"
#include "draco/core/draco_test_utils.h"
#include "draco/core/vector_d.h"
#include "gtest/gtest.h"

namespace draco {

using GeometryAttribute::NORMAL;
using GeometryAttribute::POSITION;
using GeometryAttribute::TEX_COORD;

TEST(PmcUtilsTest, FloatToRational) {
  pmc::Rational r1 = PmcUtils::FloatToRational(3.3f);
  EXPECT_EQ(r1.p, 33);
  EXPECT_EQ(r1.q, 10);

  pmc::Rational r2 = PmcUtils::FloatToRational(4.4f);
  EXPECT_EQ(r2.p, 22);
  EXPECT_EQ(r2.q, 5);

  pmc::Rational r0 = PmcUtils::FloatToRational(0.0f);
  EXPECT_EQ(r0.p, 0);
  EXPECT_EQ(r0.q, 1);
}

TEST(PmcUtilsTest, CreateArrayBuffer) {
  // Create a sample vector.
  std::vector<int32_t> data = {1, 2, 3, 4, 5, 6};
  const size_t components_per_vector = 3;

  // Create the array buffer.
  const pmc::ArrayBuffer buffer =
      PmcUtils::CreateArrayBuffer(data, components_per_vector);

  // Check the buffer fields.
  EXPECT_EQ(buffer.data, reinterpret_cast<uint8_t*>(data.data()));
  EXPECT_EQ(buffer.dataType, pmc::DataType::Int32);
  EXPECT_EQ(buffer.offset, 0);
  EXPECT_EQ(buffer.componentsPerVector, components_per_vector);
  EXPECT_EQ(buffer.vectorCount, data.size() / components_per_vector);
  EXPECT_EQ(buffer.stride, components_per_vector * sizeof(int32_t));
}

TEST(PmcUtilsTest, CreateGeometryMeshpart) {
  // Create geometry meshpart.
  //
  //    y
  //
  //    4       (2)-----(3)
  //            / \     /
  //           /   \   /
  //          /     \ /
  //    2   (0)-----(1)
  //
  //         3   5   7   9   x
  //
  std::vector<int32_t> positions = {3, 2, 0, 7, 2, 0, 5, 4, 0, 9, 4, 0};
  std::vector<int32_t> face_degrees = {3, 3};
  std::vector<int32_t> indices = {0, 1, 2, 1, 3, 2};

  // Create the geometry meshpart.
  constexpr int32_t kBitDepth = 10;
  const pmc::GeometryMeshpart meshpart = PmcUtils::CreateGeometryMeshpart(
      positions, face_degrees, indices, kBitDepth);

  // Check the meshpart fields.
  EXPECT_EQ(meshpart.info.vertexCount, 4);
  EXPECT_EQ(meshpart.info.faceCount, 2);
  EXPECT_EQ(meshpart.info.indexCount, 6);
  EXPECT_EQ(meshpart.info.faceType, pmc::MeshFaceType::TRIANGULAR);
  EXPECT_EQ(meshpart.info.coordSys.p, (1 << kBitDepth) - 1);
  EXPECT_EQ(meshpart.info.coordSys.q, 1);
  EXPECT_EQ(meshpart.info.connectivityRegularized, true);

  // Check the meshpart buffers fields.
  EXPECT_EQ(meshpart.buffers.positions.vectorCount, 4);
  EXPECT_EQ(meshpart.buffers.faceDegrees.vectorCount, 2);
  EXPECT_EQ(meshpart.buffers.indices.vectorCount, 6);
}

TEST(PmcUtilsTest, CreateAttributeMeshpart) {
  // Create sample vectors for texture coordinates.
  std::vector<int32_t> values = {0, 0, 1, 0, 0, 1, 1, 1};
  std::vector<int32_t> indices = {0, 1, 2, 1, 3, 2};

  // Create the attribute meshpart.
  constexpr int32_t kAttId = 0;
  constexpr int32_t kComponentsPerVector = 2;
  constexpr int32_t kBitDepth = 12;
  constexpr bool kOctahedralNormals = true;
  const pmc::AttributeMeshpart meshpart = PmcUtils::CreateAttributeMeshpart(
      values, indices, kAttId, kComponentsPerVector,
      pmc::AttributeType::TEX_COORD, pmc::AttributeScope::CORNER, kBitDepth,
      kOctahedralNormals);

  // Check the meshpart info fields.
  EXPECT_EQ(meshpart.info.vectorCount, 4);
  EXPECT_EQ(meshpart.info.attributeId, kAttId);
  EXPECT_EQ(meshpart.info.componentsPerVector, kComponentsPerVector);
  EXPECT_EQ(meshpart.info.type, pmc::AttributeType::TEX_COORD);
  EXPECT_EQ(meshpart.info.scope, pmc::AttributeScope::CORNER);
  EXPECT_EQ(meshpart.info.indexCount, 6);
  EXPECT_EQ(meshpart.info.explicitIndices, true);
  EXPECT_EQ(meshpart.info.coordSysProjection,
            pmc::CoordSysProjection::OCTAHEDRAL);
  EXPECT_EQ(meshpart.info.coordSys->scale.p, 2047);
  EXPECT_EQ(meshpart.info.coordSys->scale.q, 1);

  // Check the meshpart buffers fields.
  EXPECT_EQ(meshpart.buffers.values.vectorCount, 4);
  EXPECT_EQ(meshpart.buffers.indices.vectorCount, 6);
}

TEST(PmcUtilsTest, CreateAttributeInfo) {
  // Create sample attribute meshpart info.
  pmc::AttributeMeshpartInfo meshpart_info;
  meshpart_info.attributeId = 0;
  pmc::AttributeInfo::CoordinateSystem cs;
  cs.scale = pmc::Rational{(1 << 10) - 1, 1};
  meshpart_info.coordSys = cs;
  meshpart_info.componentsPerVector = 2;
  meshpart_info.type = pmc::AttributeType::TEX_COORD;
  meshpart_info.scope = pmc::AttributeScope::CORNER;

  // Create sample min values and scale.
  Vector2f min_vals = {1.1f, 2.2f};
  constexpr float kScale = 3.3f;

  // Create the attribute info.
  const pmc::AttributeInfo info = PmcUtils::CreateAttributeInfo(
      meshpart_info, min_vals, kScale, "TEXCOORD_1");

  // Check the attribute info fields.
  EXPECT_EQ(info.attributeId, 0);
  EXPECT_EQ(info.coordSys->scale.p, 33);
  EXPECT_EQ(info.coordSys->scale.q, 10);
  EXPECT_EQ(info.coordSys->origin[0], 4);  // round(1.1f * 3.3f == 3.63) = 4.
  EXPECT_EQ(info.coordSys->origin[1], 7);  // round(2.2f * 3.3f == 7.26) = 7.
  EXPECT_EQ(info.componentsPerVector, 2);
  EXPECT_EQ(info.type, pmc::AttributeType::TEX_COORD);
  EXPECT_EQ(info.scope, pmc::AttributeScope::CORNER);
  EXPECT_EQ(info.name, "TEXCOORD_1");
}

TEST(PmcUtilsTest, CreateGeometryInfo) {
  // Create geometry info.
  constexpr int32_t kBitDepth = 10;
  const Vector3f min_vals = {1.1f, 2.2f, 3.3f};
  const float scale = 4.4f;
  const pmc::GeometryInfo info =
      PmcUtils::CreateGeometryInfo(kBitDepth, min_vals, scale);
  EXPECT_EQ(info.coordSys.p, 22);
  EXPECT_EQ(info.coordSys.q, 5);
  EXPECT_EQ(info.coordSysOrigin[0], 5);   // round(1.1f * 4.4f == 4.84) = 5.
  EXPECT_EQ(info.coordSysOrigin[1], 10);  // round(2.2f * 4.4f == 9.68) = 10.
  EXPECT_EQ(info.coordSysOrigin[2], 15);  // round(3.3f * 4.4f == 14.52) = 15.
  EXPECT_EQ(info.coordSysOriginScaleLog2[0], 0);
}

TEST(PmcUtilsTest, ComputeBoundingBox) {
  auto mesh = draco::ReadMeshFromTestFile("cube_att.obj");
  ASSERT_NE(mesh, nullptr);

  // Compute and check position bounding box.
  BoundingBox position_bbox = PmcUtils::ComputeBoundingBox<float, 3>(
      *mesh->GetNamedAttribute(POSITION));
  ASSERT_EQ(position_bbox.GetMinPoint()[0], 0.0f);
  ASSERT_EQ(position_bbox.GetMinPoint()[1], 0.0f);
  ASSERT_EQ(position_bbox.GetMinPoint()[2], 0.0f);
  ASSERT_EQ(position_bbox.GetMaxPoint()[0], 1.0f);
  ASSERT_EQ(position_bbox.GetMaxPoint()[1], 1.0f);
  ASSERT_EQ(position_bbox.GetMaxPoint()[2], 1.0f);

  // Compute and check texture coordinate bounding box.
  BoundingBoxD<float, 2> tex_coord_bbox =
      PmcUtils::ComputeBoundingBox<float, 2>(
          *mesh->GetNamedAttribute(TEX_COORD));
  ASSERT_EQ(tex_coord_bbox.GetMinPoint()[0], 0.0f);
  ASSERT_EQ(tex_coord_bbox.GetMinPoint()[1], 0.0f);
  ASSERT_EQ(tex_coord_bbox.GetMaxPoint()[0], 1.0f);
  ASSERT_EQ(tex_coord_bbox.GetMaxPoint()[1], 1.0f);

  // Compute and check normal bounding box.
  BoundingBox normal_bbox =
      PmcUtils::ComputeBoundingBox<float, 3>(*mesh->GetNamedAttribute(NORMAL));
  ASSERT_EQ(normal_bbox.GetMinPoint()[0], -1.0f);
  ASSERT_EQ(normal_bbox.GetMinPoint()[1], -1.0f);
  ASSERT_EQ(normal_bbox.GetMinPoint()[2], -1.0f);
  ASSERT_EQ(normal_bbox.GetMaxPoint()[0], +1.0f);
  ASSERT_EQ(normal_bbox.GetMaxPoint()[1], +1.0f);
  ASSERT_EQ(normal_bbox.GetMaxPoint()[2], +1.0f);
}

TEST(PmcUtilsTest, ComputeQuantizationScale) {
  // Create a bounding box.
  BoundingBox bbox;
  bbox.Update({0.0f, 0.0f, 0.0f});
  bbox.Update({1.0f, 1.0f, 1.0f});

  // Compute scale for various quantization bits.
  ASSERT_EQ(PmcUtils::ComputeQuantizationScale(bbox, 8), 255.0f);
  ASSERT_EQ(PmcUtils::ComputeQuantizationScale(bbox, 10), 1023.0f);
  ASSERT_EQ(PmcUtils::ComputeQuantizationScale(bbox, 12), 4095.0f);

  // Extend bounding box.
  bbox.Update({-1.0f, -7.0f, 4.0f});

  // Check bounding box size.
  ASSERT_EQ(bbox.Size()[0], 2.0f);
  ASSERT_EQ(bbox.Size()[1], 8.0f);
  ASSERT_EQ(bbox.Size()[2], 4.0f);

  // Compute and check scale for various quantization bits.
  ASSERT_EQ(PmcUtils::ComputeQuantizationScale(bbox, 8), 255.0f / 8.0f);
  ASSERT_EQ(PmcUtils::ComputeQuantizationScale(bbox, 10), 1023.0f / 8.0f);
  ASSERT_EQ(PmcUtils::ComputeQuantizationScale(bbox, 12), 4095.0f / 8.0f);
}

TEST(PmcUtilsTest, ComputeQuantizationScale2D) {
  // Create a bounding box.
  BoundingBoxD<float, 2> bbox;
  bbox.Update({0.0f, 0.0f});
  bbox.Update({1.0f, 1.0f});

  // Compute scale for various quantization bits.
  ASSERT_EQ(PmcUtils::ComputeQuantizationScale(bbox, 8), 255.0f);
  ASSERT_EQ(PmcUtils::ComputeQuantizationScale(bbox, 10), 1023.0f);
  ASSERT_EQ(PmcUtils::ComputeQuantizationScale(bbox, 12), 4095.0f);

  // Extend bounding box.
  bbox.Update({-1.0f, -7.0f});

  // Check bounding box size.
  ASSERT_EQ(bbox.Size()[0], 2.0f);
  ASSERT_EQ(bbox.Size()[1], 8.0f);

  // Compute and check scale for various quantization bits.
  ASSERT_EQ(PmcUtils::ComputeQuantizationScale(bbox, 8), 255.0f / 8.0f);
  ASSERT_EQ(PmcUtils::ComputeQuantizationScale(bbox, 10), 1023.0f / 8.0f);
  ASSERT_EQ(PmcUtils::ComputeQuantizationScale(bbox, 12), 4095.0f / 8.0f);
}

TEST(PmcUtilsTest, QuantizePos) {
  constexpr float kMinVal = 0.5f;
  constexpr float kScale = 10.0f;
  EXPECT_EQ(PmcUtils::Quantize(0.5f, kMinVal, kScale), 0);
  EXPECT_EQ(PmcUtils::Quantize(1.0f, kMinVal, kScale), 5);
  EXPECT_EQ(PmcUtils::Quantize(1.1f, kMinVal, kScale), 6);
}

TEST(PmcUtilsTest, QuantizeTexcoord) {
  constexpr float kMinVal = 0.0f;
  constexpr float kScale = 1.0f;
  EXPECT_EQ(PmcUtils::Quantize(0.0f, kMinVal, kScale), 0);
  EXPECT_EQ(PmcUtils::Quantize(1.0f, kMinVal, kScale), 1);
  EXPECT_EQ(PmcUtils::Quantize(1.1f, kMinVal, kScale), 1);
}

TEST(PmcUtilsTest, QuantizeNormal) {
  constexpr float kMinVal = -1.0f;
  constexpr float kScale = 1.0f;
  EXPECT_EQ(PmcUtils::Quantize(0.0f, kMinVal, kScale), 1);
  EXPECT_EQ(PmcUtils::Quantize(1.0f, kMinVal, kScale), 2);
  EXPECT_EQ(PmcUtils::Quantize(1.1f, kMinVal, kScale), 2);
}

TEST(PmcUtilsTest, QuantizeOctahedralNormal) {
  constexpr int32_t kQbits = 10;
  int32_t quantized[3];

  {
    // Test [1, 0, 0].
    const float normal[3] = {1.0f, 0.0f, 0.0f};
    PmcUtils::QuantizeOctahedralNormal(normal, kQbits, quantized);
    EXPECT_EQ(quantized[0], 511);
    EXPECT_EQ(quantized[1], 0);
    EXPECT_EQ(quantized[2], 0);
  }

  {
    // Test [-1, 0, 0].
    const float normal[3] = {-1.0f, 0.0f, 0.0f};
    PmcUtils::QuantizeOctahedralNormal(normal, kQbits, quantized);
    EXPECT_EQ(quantized[0], -511);
    EXPECT_EQ(quantized[1], 0);
    EXPECT_EQ(quantized[2], 0);
  }

  {
    // Test [0, 0, 1].
    const float normal[3] = {0.0f, 0.0f, 1.0f};
    PmcUtils::QuantizeOctahedralNormal(normal, kQbits, quantized);
    EXPECT_EQ(quantized[0], 0);
    EXPECT_EQ(quantized[1], 0);
    EXPECT_EQ(quantized[2], 511);
  }

  {
    // Test [0.8, 0, 0.6].
    const float normal[3] = {0.8f, 0.0f, 0.6f};
    PmcUtils::QuantizeOctahedralNormal(normal, kQbits, quantized);
    EXPECT_EQ(quantized[0], 292);
    EXPECT_EQ(quantized[1], 0);
    EXPECT_EQ(quantized[2], 219);
  }

  {
    // Test [0, 0, 0].
    const float normal[3] = {0.0f, 0.0f, 0.0f};
    PmcUtils::QuantizeOctahedralNormal(normal, kQbits, quantized);
    EXPECT_EQ(quantized[0], 509);
    EXPECT_EQ(quantized[1], 1);
    EXPECT_EQ(quantized[2], 1);
  }
}

TEST(PmcUtilsTest, DequantizeOctahedralNormal) {
  float dequantized[3];

  {
    // Test [1, 0, 0].
    const int32_t quantized[3] = {511, 0, 0};
    PmcUtils::DequantizeOctahedralNormal(quantized, dequantized);
    EXPECT_NEAR(dequantized[0], 1.0f, 1e-6f);
    EXPECT_NEAR(dequantized[1], 0.0f, 1e-6f);
    EXPECT_NEAR(dequantized[2], 0.0f, 1e-6f);
  }

  {
    // Test [-1, 0, 0].
    const int32_t quantized[3] = {-511, 0, 0};
    PmcUtils::DequantizeOctahedralNormal(quantized, dequantized);
    EXPECT_NEAR(dequantized[0], -1.0f, 1e-6f);
    EXPECT_NEAR(dequantized[1], 0.0f, 1e-6f);
    EXPECT_NEAR(dequantized[2], 0.0f, 1e-6f);
  }

  {
    // Test [0, 0, 1].
    const int32_t quantized[3] = {0, 0, 511};
    PmcUtils::DequantizeOctahedralNormal(quantized, dequantized);
    EXPECT_NEAR(dequantized[0], 0.0f, 1e-6f);
    EXPECT_NEAR(dequantized[1], 0.0f, 1e-6f);
    EXPECT_NEAR(dequantized[2], 1.0f, 1e-6f);
  }

  {
    // Test [0.8, 0, 0.6].
    const int32_t quantized[3] = {292, 0, 219};
    PmcUtils::DequantizeOctahedralNormal(quantized, dequantized);
    EXPECT_NEAR(dequantized[0], 0.8f, 1e-2f);
    EXPECT_NEAR(dequantized[1], 0.0f, 1e-2f);
    EXPECT_NEAR(dequantized[2], 0.6f, 1e-2f);
  }
}

TEST(PmcUtilsTest, OctahedralNormalRoundTrip) {
  constexpr int32_t kQbits = 10;
  const std::vector<std::vector<float>> test_normals = {
      {+1.000000f, +0.000000f, +0.000000f},
      {+0.000000f, +1.000000f, +0.000000f},
      {+0.000000f, +0.000000f, +1.000000f},
      {-1.000000f, +0.000000f, +0.000000f},
      {+0.800000f, +0.000000f, +0.600000f},
      {+0.577350f, +0.577350f, +0.577350f},
      {-0.577350f, -0.577350f, -0.577350f},
      {+0.707107f, +0.707107f, +0.000000f},
      {-0.707107f, +0.000000f, +0.707107f}};

  for (const auto& original_vec : test_normals) {
    const float original[3] = {original_vec[0], original_vec[1],
                               original_vec[2]};
    int32_t quantized[3];
    float reconstructed[3];

    PmcUtils::QuantizeOctahedralNormal(original, kQbits, quantized);
    PmcUtils::DequantizeOctahedralNormal(quantized, reconstructed);

    // Check that reconstructed normal is unit length.
    const float length = std::sqrt(reconstructed[0] * reconstructed[0] +
                                   reconstructed[1] * reconstructed[1] +
                                   reconstructed[2] * reconstructed[2]);
    EXPECT_NEAR(length, 1.0f, 1e-6f);

    // Check that reconstructed normal is close to the original.
    // Octahedral quantization with 10 bits should be fairly accurate.
    EXPECT_NEAR(reconstructed[0], original[0], 0.01f);
    EXPECT_NEAR(reconstructed[1], original[1], 0.01f);
    EXPECT_NEAR(reconstructed[2], original[2], 0.01f);
  }
}

TEST(PmcUtilsTest, DequantizePos) {
  constexpr float kMinVal = 0.5f;
  constexpr float kScale = 10.0f;
  EXPECT_EQ(PmcUtils::Dequantize(0, kMinVal, kScale), 0.5f);
  EXPECT_EQ(PmcUtils::Dequantize(5, kMinVal, kScale), 1.0f);
}

TEST(PmcUtilsTest, DequantizeTexcoord) {
  constexpr float kMinVal = 0.0f;
  constexpr float kScale = 1.0f;
  EXPECT_EQ(PmcUtils::Dequantize(0, kMinVal, kScale), 0.0f);
  EXPECT_EQ(PmcUtils::Dequantize(1, kMinVal, kScale), 1.0f);
}

TEST(PmcUtilsTest, DequantizeNormal) {
  constexpr float kMinVal = -1.0f;
  constexpr float kScale = 1.0f;
  EXPECT_EQ(PmcUtils::Dequantize(1, kMinVal, kScale), 0.0f);
  EXPECT_EQ(PmcUtils::Dequantize(2, kMinVal, kScale), 1.0f);
}

}  // namespace draco

#endif  // DRACO_PMC_SUPPORTED
