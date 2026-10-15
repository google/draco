// Copyright 2024 The Draco Authors.
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
#include "draco/metadata/property_texture.h"

#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "draco/core/draco_test_base.h"
#include "draco/core/draco_test_utils.h"
#include "draco/texture/texture.h"
#include "draco/texture/texture_library.h"
#include "draco/texture/texture_map.h"

namespace {

#ifdef DRACO_TRANSCODER_SUPPORTED

TEST(PropertyTextureTest, TestPropertyDefaults) {
  // Test construction of an empty property texture property.
  draco::PropertyTexture::Property property;
  ASSERT_TRUE(property.GetName().empty());
  ASSERT_EQ(property.GetTextureMap().texture(), nullptr);
  ASSERT_EQ(property.GetTextureMap().type(), draco::TextureMap::GENERIC);
  ASSERT_TRUE(property.GetTextureChannels().empty());
}

TEST(PropertyTextureTest, TestPropertyTextureDefaults) {
  // Test construction of an empty property texture.
  draco::PropertyTexture attribute;
  ASSERT_TRUE(attribute.GetName().empty());
  ASSERT_TRUE(attribute.GetClass().empty());
  ASSERT_EQ(attribute.NumProperties(), 0);
}

TEST(PropertyTextureTest, TestPropertySettersAndGetters) {
  // Test setter and getter methods of the property texture property.
  draco::PropertyTexture::Property property;
  property.SetName("The magnitude.");
  draco::TextureMap texture_map;
  texture_map.SetProperties(draco::TextureMap::GENERIC, 1);
  std::unique_ptr<draco::Texture> texture(new draco::Texture());
  texture_map.SetTexture(texture.get());
  property.SetTextureMap(texture_map);
  const std::vector<int> channels = {2, 3};
  property.SetTextureChannels(channels);

  // Check that property members can be accessed via getters.
  ASSERT_EQ(property.GetName(), "The magnitude.");
  ASSERT_EQ(property.GetTextureMap().texture(), texture.get());
  ASSERT_EQ(property.GetTextureMap().type(), draco::TextureMap::GENERIC);
  ASSERT_EQ(property.GetTextureChannels(), channels);
}

TEST(PropertyTextureTest, TestPropertyTextureSettersAndGetters) {
  // Test setter and getter methods of the property texture.
  draco::PropertyTexture texture;
  texture.SetName("The movement.");
  texture.SetClass("movement");
  {
    std::unique_ptr<draco::PropertyTexture::Property> property(
        new draco::PropertyTexture::Property());
    property->SetName("The magnitude.");
    ASSERT_EQ(texture.AddProperty(std::move(property)), 0);
  }
  {
    std::unique_ptr<draco::PropertyTexture::Property> property(
        new draco::PropertyTexture::Property());
    property->SetName("The direction.");
    ASSERT_EQ(texture.AddProperty(std::move(property)), 1);
  }

  // Check that property texture members can be accessed via getters.
  ASSERT_EQ(texture.GetName(), "The movement.");
  ASSERT_EQ(texture.GetClass(), "movement");
  ASSERT_EQ(texture.NumProperties(), 2);
  ASSERT_EQ(texture.GetProperty(0).GetName(), "The magnitude.");
  ASSERT_EQ(texture.GetProperty(1).GetName(), "The direction.");

  // Check that properties can be removed.
  texture.RemoveProperty(0);
  ASSERT_EQ(texture.NumProperties(), 1);
  ASSERT_EQ(texture.GetProperty(0).GetName(), "The direction.");
  texture.RemoveProperty(0);
  ASSERT_EQ(texture.NumProperties(), 0);
}

TEST(PropertyTextureTest, TestPropertyCopy) {
  // Test that property texture property can be copied.
  draco::PropertyTexture::Property property;
  property.SetName("The direction.");
  draco::TextureMap texture_map;
  texture_map.SetProperties(draco::TextureMap::GENERIC, 1);
  std::unique_ptr<draco::Texture> texture(new draco::Texture());
  texture_map.SetTexture(texture.get());
  property.SetTextureMap(texture_map);
  const std::vector<int> channels = {2, 3};
  property.SetTextureChannels(channels);

  // Make a copy.
  draco::PropertyTexture::Property copy;
  copy.Copy(property);

  // Check the copy.
  ASSERT_EQ(copy.GetName(), "The direction.");
  ASSERT_EQ(copy.GetTextureMap().texture(), texture.get());
  ASSERT_EQ(copy.GetTextureMap().type(), draco::TextureMap::GENERIC);
  ASSERT_EQ(copy.GetTextureChannels(), channels);
}

TEST(PropertyTextureTest, TestPropertyTextureCopy) {
  // Test that property texture can be copied.
  draco::PropertyTexture texture;
  texture.SetName("The movement.");
  texture.SetClass("movement");
  {
    std::unique_ptr<draco::PropertyTexture::Property> property(
        new draco::PropertyTexture::Property());
    property->SetName("The magnitude.");
    ASSERT_EQ(texture.AddProperty(std::move(property)), 0);
  }
  {
    std::unique_ptr<draco::PropertyTexture::Property> property(
        new draco::PropertyTexture::Property());
    property->SetName("The direction.");
    ASSERT_EQ(texture.AddProperty(std::move(property)), 1);
  }

  // Make a copy.
  draco::PropertyTexture copy;
  copy.Copy(texture);

  // Check the copy.
  ASSERT_EQ(texture.GetName(), "The movement.");
  ASSERT_EQ(texture.GetClass(), "movement");
  ASSERT_EQ(texture.NumProperties(), 2);
  ASSERT_EQ(texture.GetProperty(0).GetName(), "The magnitude.");
  ASSERT_EQ(texture.GetProperty(1).GetName(), "The direction.");
}

#endif  // DRACO_TRANSCODER_SUPPORTED

}  // namespace
