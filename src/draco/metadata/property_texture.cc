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

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "draco/texture/texture.h"
#include "draco/texture/texture_library.h"
#include "draco/texture/texture_map.h"

#ifdef DRACO_TRANSCODER_SUPPORTED

namespace draco {

bool PropertyTexture::Property::operator==(const Property& other) const {
  // Texture map contents are ignored.
  return name_ == other.name_ && texture_channels_ == other.texture_channels_;
}

void PropertyTexture::Property::Copy(const Property& src) {
  name_ = src.name_;
  texture_map_.Copy(src.texture_map_);
  texture_channels_ = src.texture_channels_;
}

void PropertyTexture::Property::SetName(const std::string& name) {
  name_ = name;
}

const std::string& PropertyTexture::Property::GetName() const { return name_; }

void PropertyTexture::Property::SetTextureMap(const TextureMap& texture_map) {
  texture_map_.Copy(texture_map);
}

void PropertyTexture::Property::SetTextureMap(Texture* texture,
                                              int tex_coord_index) {
  texture_map_.SetProperties(TextureMap::GENERIC, tex_coord_index);
  texture_map_.SetTexture(texture);
}

const TextureMap& PropertyTexture::Property::GetTextureMap() const {
  return texture_map_;
}

TextureMap& PropertyTexture::Property::GetTextureMap() { return texture_map_; }

void PropertyTexture::Property::SetTextureChannels(
    const std::vector<int>& texture_channels) {
  texture_channels_ = texture_channels;
}

const std::vector<int>& PropertyTexture::Property::GetTextureChannels() const {
  return texture_channels_;
}

std::vector<int>& PropertyTexture::Property::GetTextureChannels() {
  return texture_channels_;
}

void PropertyTexture::Property::UpdateTexturePointer(
    const std::unordered_map<const Texture*, int>& texture_to_index_map,
    TextureLibrary* texture_library) {
  const auto it = texture_to_index_map.find(texture_map_.texture());
  const int texture_index = it->second;
  texture_map_.SetTexture(texture_library->GetTexture(texture_index));
}

bool PropertyTexture::operator==(const PropertyTexture& other) const {
  if (name_ != other.name_ || class_ != other.class_ ||
      properties_.size() != other.properties_.size()) {
    return false;
  }
  for (int i = 0; i < properties_.size(); ++i) {
    if (*properties_[i] != *other.properties_[i]) {
      return false;
    }
  }
  return true;
}

void PropertyTexture::Copy(const PropertyTexture& src) {
  name_ = src.name_;
  class_ = src.class_;
  properties_.clear();
  properties_.reserve(src.properties_.size());
  for (int i = 0; i < src.properties_.size(); ++i) {
    std::unique_ptr<Property> property(new Property());
    property->Copy(src.GetProperty(i));
    properties_.push_back(std::move(property));
  }
}

void PropertyTexture::SetName(const std::string& value) { name_ = value; }

const std::string& PropertyTexture::GetName() const { return name_; }

void PropertyTexture::SetClass(const std::string& value) { class_ = value; }

const std::string& PropertyTexture::GetClass() const { return class_; }

int PropertyTexture::AddProperty(std::unique_ptr<Property> property) {
  properties_.push_back(std::move(property));
  return properties_.size() - 1;
}

int PropertyTexture::NumProperties() const { return properties_.size(); }

const PropertyTexture::Property& PropertyTexture::GetProperty(int index) const {
  return *properties_[index];
}

PropertyTexture::Property& PropertyTexture::GetProperty(int index) {
  return *properties_[index];
}

void PropertyTexture::RemoveProperty(int index) {
  properties_.erase(properties_.begin() + index);
}

}  // namespace draco

#endif  // DRACO_TRANSCODER_SUPPORTED
