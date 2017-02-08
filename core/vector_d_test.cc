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
#include "core/vector_d.h"

#include "core/draco_test_base.h"

namespace {

typedef draco::Vector2f Vector2f;
typedef draco::Vector3f Vector3f;
typedef draco::Vector4f Vector4f;
typedef draco::Vector5f Vector5f;
typedef draco::Vector2ui Vector2ui;
typedef draco::Vector3ui Vector3ui;
typedef draco::Vector4ui Vector4ui;
typedef draco::Vector5ui Vector5ui;

class VectorDTest : public ::testing::Test {
 protected:
  template <class CoeffT, int dimension_t>
  void TestSquaredDistance(const draco::VectorD<CoeffT, dimension_t> v1,
                           const draco::VectorD<CoeffT, dimension_t> v2,
                           const CoeffT result) {
    CoeffT squared_distance = SquaredDistance(v1, v2);
    ASSERT_EQ(squared_distance, result);
    squared_distance = SquaredDistance(v2, v1);
    ASSERT_EQ(squared_distance, result);
  }
};

TEST_F(VectorDTest, TestOperators) {
  {
    const Vector3f v;
    ASSERT_EQ(v[0], 0);
    ASSERT_EQ(v[1], 0);
    ASSERT_EQ(v[2], 0);
  }
  const Vector3f v(1, 2, 3);
  ASSERT_EQ(v[0], 1);
  ASSERT_EQ(v[1], 2);
  ASSERT_EQ(v[2], 3);

  Vector3f w = v;
  bool comp = (v == w);
  ASSERT_TRUE(comp);
  comp = (v != w);
  ASSERT_TRUE(!comp);
  ASSERT_EQ(w[0], 1);
  ASSERT_EQ(w[1], 2);
  ASSERT_EQ(w[2], 3);

  w = -v;
  ASSERT_EQ(w[0], -1);
  ASSERT_EQ(w[1], -2);
  ASSERT_EQ(w[2], -3);

  w = v + v;
  ASSERT_EQ(w[0], 2);
  ASSERT_EQ(w[1], 4);
  ASSERT_EQ(w[2], 6);

  w = w - v;
  ASSERT_EQ(w[0], 1);
  ASSERT_EQ(w[1], 2);
  ASSERT_EQ(w[2], 3);

  w = v * float(2);
  ASSERT_EQ(w[0], 2);
  ASSERT_EQ(w[1], 4);
  ASSERT_EQ(w[2], 6);

  ASSERT_EQ(v.SquaredNorm(), 14);
  ASSERT_EQ(v.Dot(v), 14);
}

TEST_F(VectorDTest, TestSquaredDistance) {
  // Test Vector2f: float, 2D.
  Vector2f v1_2f(5.5, 10.5);
  Vector2f v2_2f(3.5, 15.5);
  float result_f = 29;
  TestSquaredDistance(v1_2f, v2_2f, result_f);

  // Test Vector3f: float, 3D.
  Vector3f v1_3f(5.5, 10.5, 2.3);
  Vector3f v2_3f(3.5, 15.5, 0);
  result_f = 34.29;
  TestSquaredDistance(v1_3f, v2_3f, result_f);

  // Test Vector4f: float, 4D.
  Vector4f v1_4f(5.5, 10.5, 2.3, 7.2);
  Vector4f v2_4f(3.5, 15.5, 0, 9.9);
  result_f = 41.58;
  TestSquaredDistance(v1_4f, v2_4f, result_f);

  // Test Vector5f: float, 5D.
  Vector5f v1_5f(5.5, 10.5, 2.3, 7.2, 1.0);
  Vector5f v2_5f(3.5, 15.5, 0, 9.9, 0.2);
  result_f = 42.22;
  TestSquaredDistance(v1_5f, v2_5f, result_f);

  // Test Vector 2ui: uint32_t, 2D.
  Vector2ui v1_2ui(5, 10);
  Vector2ui v2_2ui(3, 15);
  uint32_t result_ui = 29;
  TestSquaredDistance(v1_2ui, v2_2ui, result_ui);

  // Test Vector 3ui: uint32_t, 3D.
  Vector3ui v1_3ui(5, 10, 2);
  Vector3ui v2_3ui(3, 15, 0);
  result_ui = 33;
  TestSquaredDistance(v1_3ui, v2_3ui, result_ui);

  // Test Vector 4ui: uint32_t, 4D.
  Vector4ui v1_4ui(5, 10, 2, 7);
  Vector4ui v2_4ui(3, 15, 0, 9);
  result_ui = 37;
  TestSquaredDistance(v1_4ui, v2_4ui, result_ui);

  // Test Vector 5ui: uint32_t, 5D.
  Vector5ui v1_5ui(5, 10, 2, 7, 1);
  Vector5ui v2_5ui(3, 15, 0, 9, 12);
  result_ui = 158;
  TestSquaredDistance(v1_5ui, v2_5ui, result_ui);
}

}  // namespace
