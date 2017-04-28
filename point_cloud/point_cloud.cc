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
#include "point_cloud/point_cloud.h"

#include <unordered_map>

namespace draco {

PointCloud::PointCloud() : num_points_(0) {}

int32_t PointCloud::NumNamedAttributes(GeometryAttribute::Type type) const {
  if (type == GeometryAttribute::INVALID ||
      type >= GeometryAttribute::NAMED_ATTRIBUTES_COUNT)
    return 0;
  return named_attribute_index_[type].size();
}

int32_t PointCloud::GetNamedAttributeId(GeometryAttribute::Type type) const {
  return GetNamedAttributeId(type, 0);
}

int32_t PointCloud::GetNamedAttributeId(GeometryAttribute::Type type,
                                        int i) const {
  if (NumNamedAttributes(type) <= i)
    return -1;
  return named_attribute_index_[type][i];
}

const PointAttribute *PointCloud::GetNamedAttribute(
    GeometryAttribute::Type type) const {
  return GetNamedAttribute(type, 0);
}

const PointAttribute *PointCloud::GetNamedAttribute(
    GeometryAttribute::Type type, int i) const {
  const int32_t att_id = GetNamedAttributeId(type, i);
  if (att_id == -1)
    return nullptr;
  return attributes_[att_id].get();
}

const PointAttribute *PointCloud::GetNamedAttributeByCustomId(
    GeometryAttribute::Type type, uint16_t custom_id) const {
  for (size_t att_id = 0; att_id < named_attribute_index_[type].size();
       ++att_id) {
    if (attributes_[named_attribute_index_[type][att_id]]->custom_id() ==
        custom_id)
      return attributes_[named_attribute_index_[type][att_id]].get();
  }
  return nullptr;
}

int PointCloud::AddAttribute(std::unique_ptr<PointAttribute> pa) {
  SetAttribute(attributes_.size(), std::move(pa));
  return attributes_.size() - 1;
}

int PointCloud::AddAttribute(
    const GeometryAttribute &att, bool identity_mapping,
    AttributeValueIndex::ValueType num_attribute_values) {
  const GeometryAttribute::Type type = att.attribute_type();
  if (type == GeometryAttribute::INVALID)
    return -1;
  const int32_t att_id =
      AddAttribute(std::unique_ptr<PointAttribute>(new PointAttribute(att)));
  // Initialize point cloud specific attribute data.
  if (!identity_mapping) {
    // First create mapping between indices.
    attribute(att_id)->SetExplicitMapping(num_points_);
  } else {
    attribute(att_id)->SetIdentityMapping();
    attribute(att_id)->Resize(num_points_);
  }
  if (num_attribute_values > 0) {
    attribute(att_id)->Reset(num_attribute_values);
  }
  return att_id;
}

void PointCloud::SetAttribute(int att_id, std::unique_ptr<PointAttribute> pa) {
  DCHECK(att_id >= 0);
  if (static_cast<int>(attributes_.size()) <= att_id) {
    attributes_.resize(att_id + 1);
  }
  if (pa->attribute_type() < GeometryAttribute::NAMED_ATTRIBUTES_COUNT) {
    named_attribute_index_[pa->attribute_type()].push_back(att_id);
  }
  attributes_[att_id] = std::move(pa);
}

void PointCloud::DeduplicatePointIds() {
  // Hashing function for a single vertex.
  auto point_hash = [this](PointIndex p) {
    PointIndex::ValueType hash = 0;
    for (int32_t i = 0; i < this->num_attributes(); ++i) {
      const AttributeValueIndex att_id = attribute(i)->mapped_index(p);
      hash = HashCombine(att_id.value(), hash);
    }
    return hash;
  };
  // Comparison function between two vertices.
  auto point_compare = [this](PointIndex p0, PointIndex p1) {
    for (int32_t i = 0; i < this->num_attributes(); ++i) {
      const AttributeValueIndex att_id0 = attribute(i)->mapped_index(p0);
      const AttributeValueIndex att_id1 = attribute(i)->mapped_index(p1);
      if (att_id0 != att_id1)
        return false;
    }
    return true;
  };

  std::unordered_map<PointIndex, PointIndex, decltype(point_hash),
                     decltype(point_compare)>
      unique_point_map(num_points_, point_hash, point_compare);
  int32_t num_unique_points = 0;
  IndexTypeVector<PointIndex, PointIndex> index_map(num_points_);
  std::vector<PointIndex> unique_points;
  // Go through all vertices and find their duplicates.
  for (PointIndex i(0); i < num_points_; ++i) {
    const auto it = unique_point_map.find(i);
    if (it != unique_point_map.end()) {
      index_map[i] = it->second;
    } else {
      unique_point_map.insert(std::make_pair(i, PointIndex(num_unique_points)));
      index_map[i] = num_unique_points++;
      unique_points.push_back(i);
    }
  }
  if (num_unique_points == num_points_)
    return;  // All vertices are already unique.

  ApplyPointIdDeduplication(index_map, unique_points);
  set_num_points(num_unique_points);
}

void PointCloud::ApplyPointIdDeduplication(
    const IndexTypeVector<PointIndex, PointIndex> &id_map,
    const std::vector<PointIndex> &unique_point_ids) {
  int32_t num_unique_points = 0;
  for (PointIndex i : unique_point_ids) {
    const PointIndex new_point_id = id_map[i];
    if (new_point_id >= num_unique_points) {
      // New unique vertex reached. Copy attribute indices to the proper
      // position.
      for (int32_t a = 0; a < num_attributes(); ++a) {
        attribute(a)->SetPointMapEntry(new_point_id,
                                       attribute(a)->mapped_index(i));
      }
      num_unique_points = new_point_id.value() + 1;
    }
  }
  for (int32_t a = 0; a < num_attributes(); ++a) {
    attribute(a)->SetExplicitMapping(num_unique_points);
  }
}

bool PointCloud::DeduplicateAttributeValues() {
  // Go over all attributes and create mapping between duplicate entries.
  if (num_points() == 0)
    return false;  // Unexpected attribute size.
  // Deduplicate all attributes.
  for (int32_t att_id = 0; att_id < num_attributes(); ++att_id) {
    if (!attribute(att_id)->DeduplicateValues(*attribute(att_id)))
      return false;
  }
  return true;
}

}  // namespace draco
