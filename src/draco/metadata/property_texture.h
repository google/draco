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
#ifndef DRACO_METADATA_PROPERTY_TEXTURE_H_
#define DRACO_METADATA_PROPERTY_TEXTURE_H_

#include "draco/draco_features.h"

#ifdef DRACO_TRANSCODER_SUPPORTED

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "draco/texture/texture.h"
#include "draco/texture/texture_library.h"
#include "draco/texture/texture_map.h"

namespace draco {

// Describes a property texture as defined in the EXT_structural_metadata glTF
// extension.
class PropertyTexture {
 public:
  // Describes where property is stored (in texture channels).
  class Property {
   public:
    // Creates an empty property.
    Property() = default;

    // Methods for comparing two properties.
    bool operator==(const Property& other) const;
    bool operator!=(const Property& other) const { return !(*this == other); }

    // Copies all data from |src| property.
    void Copy(const Property& src);

    // Name of this property.
    void SetName(const std::string& name);
    const std::string& GetName() const;

    // Texture map and texture channels of a property texture. See the glTF
    // extension documentation for reconstruction of property values from the
    // channel values.
    void SetTextureMap(const TextureMap& texture_map);
    void SetTextureMap(Texture* texture, int tex_coord_index);
    const TextureMap& GetTextureMap() const;
    TextureMap& GetTextureMap();
    void SetTextureChannels(const std::vector<int>& texture_channels);
    const std::vector<int>& GetTextureChannels() const;
    std::vector<int>& GetTextureChannels();

    // Updates texture pointer to point to a new |texture_library|. The current
    // texture pointer is used to determine the texture index in the new texture
    // library via a given |texture_to_index_map|.
    void UpdateTexturePointer(
        const std::unordered_map<const Texture*, int>& texture_to_index_map,
        TextureLibrary* texture_library);

   private:
    // Name of this property as in structural metadata schema class property.
    std::string name_;

    // Texture of the property texture. Various properties can be stored in a
    // common texture but they could also be stored in separate textures.
    TextureMap texture_map_;

    // Texture channels containing property values, identified by index.
    std::vector<int> texture_channels_;

    // TODO(vytyaz): Support property value modifiers min, max, offset, scale.
  };

  // Creates an empty property texture.
  PropertyTexture() = default;

  // Methods for comparing two property textures.
  bool operator==(const PropertyTexture& other) const;
  bool operator!=(const PropertyTexture& other) const {
    return !(*this == other);
  }

  // Copies all data from |src| property texture.
  void Copy(const PropertyTexture& src);

  // Name of this property texture.
  void SetName(const std::string& value);
  const std::string& GetName() const;

  // Class of this property texture.
  void SetClass(const std::string& value);
  const std::string& GetClass() const;

  // Properties.
  int AddProperty(std::unique_ptr<Property> property);
  int NumProperties() const;
  const Property& GetProperty(int index) const;
  Property& GetProperty(int index);
  void RemoveProperty(int index);

 private:
  // The name of the property texture, e.g., for display purposes.
  std::string name_;

  // The class in structural metadata schema that property values conform to.
  std::string class_;

  // Properties corresponding to schema class properties, describing where the
  // property values are stored (in texture channels).
  std::vector<std::unique_ptr<Property>> properties_;
};

}  // namespace draco

#endif  // DRACO_TRANSCODER_SUPPORTED
#endif  // DRACO_METADATA_PROPERTY_TEXTURE_H_
