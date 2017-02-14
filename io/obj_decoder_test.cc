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
#include <sstream>

#include "core/draco_test_base.h"
#include "core/draco_test_utils.h"
#include "io/obj_decoder.h"

namespace draco {

class ObjDecoderTest : public ::testing::Test {
 protected:
  template <class Geometry>
  std::unique_ptr<Geometry> DecodeObj(const std::string &file_name) const {
    const std::string path = GetTestFileFullPath(file_name);
    ObjDecoder decoder;
    std::unique_ptr<Geometry> geometry(new Geometry());
    if (!decoder.DecodeFromFile(path, geometry.get()))
      return nullptr;
    return geometry;
  }

  void test_decoding(const std::string &file_name) {
    const std::unique_ptr<Mesh> mesh(DecodeObj<Mesh>(file_name));
    ASSERT_NE(mesh, nullptr) << "Failed to load test model " << file_name;
    ASSERT_GT(mesh->num_faces(), 0);

    const std::unique_ptr<PointCloud> pc(DecodeObj<PointCloud>(file_name));
    ASSERT_NE(pc, nullptr) << "Failed to load test model " << file_name;
    ASSERT_GT(pc->num_points(), 0u);
  }
};

TEST_F(ObjDecoderTest, ExtraVertexOBJ) {
  const std::string file_name = "extra_vertex.obj";
  test_decoding(file_name);
}

TEST_F(ObjDecoderTest, ParialAttributesOBJ) {
  const std::string file_name = "cube_att_partial.obj";
  test_decoding(file_name);
}


TEST_F(ObjDecoderTest, SubObjects) {
  // Tests loading an Obj with sub objects.
  const std::string file_name = "cube_att_sub_o.obj";
  const std::unique_ptr<Mesh> mesh(DecodeObj<Mesh>(file_name));
  ASSERT_NE(mesh, nullptr) << "Failed to load test model " << file_name;
  ASSERT_GT(mesh->num_faces(), 0);

  // A sub object attribute should be the fourth attribute of the mesh (in this
  // case).
  ASSERT_EQ(mesh->num_attributes(), 4);
  ASSERT_EQ(mesh->attribute(3)->attribute_type(), GeometryAttribute::GENERIC);
  // There should be 3 different sub objects used in the model.
  ASSERT_EQ(mesh->attribute(3)->size(), 3);
  // Verify that the sub object attribute has custom id == 1.
  ASSERT_EQ(mesh->attribute(3)->custom_id(), 1);
}

}  // namespace draco
