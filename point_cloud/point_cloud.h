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
#ifndef DRACO_POINT_CLOUD_POINT_CLOUD_H_
#define DRACO_POINT_CLOUD_POINT_CLOUD_H_

#include "point_cloud/point_attribute.h"

namespace draco {

// PointCloud is a collection of n-dimensional points that are described by a
// set of PointAttributes that can represent data such as positions or colors
// of individual points (see point_attribute.h).
class PointCloud {
 public:
  PointCloud();
  virtual ~PointCloud() = default;

  // Returns the number of named attributes of a given type.
  int32_t NumNamedAttributes(GeometryAttribute::Type type) const;

  // Returns attribute id of the first named attribute with a given type or -1
  // when the attribute is not used by the point cloud.
  int32_t GetNamedAttributeId(GeometryAttribute::Type type) const;

  // Returns the id of the i-th named attribute of a given type.
  int32_t GetNamedAttributeId(GeometryAttribute::Type type, int i) const;

  // Returns the first named attribute of a given type or nullptr if the
  // attribute is not used by the point cloud.
  const PointAttribute *GetNamedAttribute(GeometryAttribute::Type type) const;

  // Returns the i-th named attribute of a given type.
  const PointAttribute *GetNamedAttribute(GeometryAttribute::Type type,
                                          int i) const;

  // Returns the named attribute of a given custom id.
  const PointAttribute *GetNamedAttributeByCustomId(
      GeometryAttribute::Type type, uint16_t id) const;

  int32_t num_attributes() const { return attributes_.size(); }
  const PointAttribute *attribute(int32_t att_id) const {
    DCHECK_LE(0, att_id);
    DCHECK_LT(att_id, static_cast<int32_t>(attributes_.size()));
    return attributes_[att_id].get();
  }

  // Returned attribute can be modified, but it's caller's responsibility to
  // maintain the attribute's consistency with draco::PointCloud.
  PointAttribute *attribute(int32_t att_id) {
    DCHECK_LE(0, att_id);
    DCHECK_LT(att_id, static_cast<int32_t>(attributes_.size()));
    return attributes_[att_id].get();
  }

  // Adds a new attribute to the point cloud.
  // Returns the attribute id.
  int AddAttribute(std::unique_ptr<PointAttribute> pa);

  // Creates and adds a new attribute to the point cloud. The attribute has
  // properties derived from the provided GeometryAttribute |att|.
  // If |identity_mapping| is set to true, the attribute will use identity
  // mapping between point indices and attribute value indices (i.e., each point
  // has a unique attribute value).
  // If |identity_mapping| is false, the mapping between point indices and
  // attribute value indices is set to explicit, and it needs to be initialized
  // manually using the PointAttribute::SetPointMapEntry() method.
  // |num_attribute_values| can be used to specify the number of attribute
  // values that are going to be stored in the newly created attribute.
  // Returns attribute id of the newly created attribute.
  int AddAttribute(const GeometryAttribute &att, bool identity_mapping,
                   AttributeValueIndex::ValueType num_attribute_values);

  // Assigns an attribute id to a given PointAttribute. If an attribute with the
  // same attribute id already exists, it is deleted.
  virtual void SetAttribute(int att_id, std::unique_ptr<PointAttribute> pa);

  // Deduplicates all attribute values (all attribute entries with the same
  // value are merged into a single entry).
  virtual bool DeduplicateAttributeValues();

  // Removes duplicate point ids (two point ids are duplicate when all of their
  // attributes are mapped to the same entry ids).
  virtual void DeduplicatePointIds();

  // Returns the number of n-dimensional points stored within the point cloud.
  size_t num_points() const { return num_points_; }

  // Sets the number of points. It's the caller's responsibility to ensure the
  // new number is valid with respect to the PointAttributes stored in the point
  // cloud.
  void set_num_points(PointIndex::ValueType num) { num_points_ = num; }

 protected:
  // Applies id mapping of deduplicated points (called by DeduplicatePointIds).
  virtual void ApplyPointIdDeduplication(
      const IndexTypeVector<PointIndex, PointIndex> &id_map,
      const std::vector<PointIndex> &unique_point_ids);

 private:
  // Attributes describing the point cloud.
  std::vector<std::unique_ptr<PointAttribute>> attributes_;

  // Ids of named attributes of the given type.
  std::vector<int32_t>
      named_attribute_index_[GeometryAttribute::NAMED_ATTRIBUTES_COUNT];

  // The number of n-dimensional points. All point attribute values are stored
  // in corresponding PointAttribute instances in the |attributes_| array.
  PointIndex::ValueType num_points_;

  friend struct PointCloudHasher;
};

// Functor for computing a hash from data stored within a point cloud.
// Note that this can be quite slow. Two point clouds will have the same hash
// only when all points have the same order and when all attribute values are
// exactly the same.
struct PointCloudHasher {
  size_t operator()(const PointCloud &pc) const {
    size_t hash = pc.num_points_;
    hash = HashCombine(pc.attributes_.size(), hash);
    for (int i = 0; i < GeometryAttribute::NAMED_ATTRIBUTES_COUNT; ++i) {
      hash = HashCombine(pc.named_attribute_index_[i].size(), hash);
      for (int j = 0; j < static_cast<int>(pc.named_attribute_index_[i].size());
           ++j) {
        hash = HashCombine(pc.named_attribute_index_[i][j], hash);
      }
    }
    // Hash attributes.
    for (int i = 0; i < static_cast<int>(pc.attributes_.size()); ++i) {
      PointAttributeHasher att_hasher;
      hash = HashCombine(att_hasher(*pc.attributes_[i]), hash);
    }
    return hash;
  }
};

}  // namespace draco

#endif  // DRACO_POINT_CLOUD_POINT_CLOUD_H_
