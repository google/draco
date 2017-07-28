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
#ifndef DRACO_METADATA_GEOMETRY_METADATA_H_
#define DRACO_METADATA_GEOMETRY_METADATA_H_

#include "draco/metadata/metadata.h"

namespace draco {

// Class for representing specifically metadata of attributes. It must have an
// attribute id which should be identical to it's counterpart attribute in
// the point cloud it belongs to.
class AttributeMetadata : public Metadata {
 public:
  AttributeMetadata(int attribute_id) : attribute_id_(attribute_id) {}
  AttributeMetadata(int attribute_id, const Metadata &metadata)
      : Metadata(metadata), attribute_id_(attribute_id) {}

  uint32_t attribute_id() const { return attribute_id_; }

 private:
  uint32_t attribute_id_;

  friend struct AttributeMetadataHasher;
};

// Functor for computing a hash from data stored in a AttributeMetadata class.
struct AttributeMetadataHasher {
  size_t operator()(const AttributeMetadata &metadata) const {
    size_t hash = metadata.attribute_id_;
    MetadataHasher metadata_hasher;
    hash = HashCombine(metadata_hasher(static_cast<const Metadata &>(metadata)),
                       hash);
    return hash;
  }
};
// Class for representing the metadata for a point cloud. It could have a list
// of attribute metadata.
class GeometryMetadata : public Metadata {
 public:
  GeometryMetadata(){};
  GeometryMetadata(const Metadata &metadata) : Metadata(metadata) {}

  const AttributeMetadata *GetAttributeMetadataByStringEntry(
      const std::string &entry_name, const std::string &entry_value) const;
  bool AddAttributeMetadata(std::unique_ptr<AttributeMetadata> att_metadata);

  const AttributeMetadata *GetAttributeMetadata(int32_t attribute_id) const {
    // TODO(zhafang): Consider using unordered_map instead of vector to store
    // attribute metadata.
    for (auto &&att_metadata : att_metadatas_) {
      if (att_metadata->attribute_id() == attribute_id) {
        return att_metadata.get();
      }
    }
    return nullptr;
  }

  AttributeMetadata *attribute_metadata(int32_t attribute_id) {
    // TODO(zhafang): Consider use unordered_map instead of vector to store
    // attribute metadata.
    for (auto &&att_metadata : att_metadatas_) {
      if (att_metadata->attribute_id() == attribute_id) {
        return att_metadata.get();
      }
    }
    return nullptr;
  }

  const std::vector<std::unique_ptr<AttributeMetadata>> &attribute_metadatas()
      const {
    return att_metadatas_;
  }

 private:
  std::vector<std::unique_ptr<AttributeMetadata>> att_metadatas_;

  friend struct GeometryMetadataHasher;
};

// Functor for computing a hash from data stored in a GeometryMetadata class.
struct GeometryMetadataHasher {
  size_t operator()(const GeometryMetadata &metadata) const {
    size_t hash = metadata.att_metadatas_.size();
    AttributeMetadataHasher att_metadata_hasher;
    for (auto &&att_metadata : metadata.att_metadatas_) {
      hash = HashCombine(att_metadata_hasher(*att_metadata), hash);
    }
    MetadataHasher metadata_hasher;
    hash = HashCombine(metadata_hasher(static_cast<const Metadata &>(metadata)),
                       hash);
    return hash;
  }
};

}  // namespace draco

#endif  // THIRD_PARTY_DRACO_METADATA_GEOMETRY_METADATA_H_
