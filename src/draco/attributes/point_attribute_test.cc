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
#include "draco/attributes/point_attribute.h"

#include "draco/core/draco_test_base.h"

namespace {

class PointAttributeTest : public ::testing::Test {
 protected:
  PointAttributeTest() {}
};

TEST_F(PointAttributeTest, TestCopy) {
  // This test verifies that PointAttribute can copy data from another point
  // attribute.
  draco::GeometryAttribute pos_att;
  pos_att.Init(draco::GeometryAttribute::POSITION, nullptr, 1, draco::DT_INT32,
               false, 4, 0);
  draco::PointAttribute pa(pos_att);
  pa.SetIdentityMapping();
  pa.Reset(10);
  for (int32_t i = 0; i < 10; ++i) {
    pa.SetAttributeValue(draco::AttributeValueIndex(i), &i);
  }

  draco::PointAttribute other_pa;
  other_pa.CopyFrom(pa);

  draco::PointAttributeHasher hasher;
  ASSERT_EQ(hasher(pa), hasher(other_pa));

  // The hash function does not actually compute the hash from attribute values,
  // so ensure the data got copied correctly as well.
  for (int32_t i = 0; i < 10; ++i) {
    int32_t data;
    other_pa.GetValue(draco::AttributeValueIndex(i), &data);
    ASSERT_EQ(data, i);
  }
}

}  // namespace
