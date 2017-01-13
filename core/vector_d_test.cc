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

using draco::Vector3f;

class VectorDTest : public ::testing::Test {
 protected:
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

}  // namespace
