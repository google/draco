// Copyright 2026 The Draco Authors.
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
#ifndef DRACO_COMPRESSION_POINT_CLOUD_POINT_CLOUD_ENCODE_STATS_H_
#define DRACO_COMPRESSION_POINT_CLOUD_POINT_CLOUD_ENCODE_STATS_H_

#include <cstddef>
#include <cstdint>
#include <vector>

#include "draco/attributes/geometry_attribute.h"

namespace draco {

// Per-attribute byte accounting recorded during PointCloudEncoder::Encode().
struct AttributeEncodeStats {
  int32_t point_attribute_id = -1;
  GeometryAttribute::Type attribute_type = GeometryAttribute::INVALID;
  uint32_t unique_id = 0;
  // Bytes written during the EncodePortableAttribute(s) phase for this
  // attribute. Populated when per_attribute_portable_bytes_exact == true (e.g.,
  // sequential point-cloud and mesh encoders); 0 when attributes are jointly
  // encoded into a single tree bitstream (e.g., k-d tree point-cloud encoder).
  size_t portable_attribute_bytes = 0;
  // Bytes written during the EncodeDataNeededByPortableTransform(s) phase for
  // this attribute (e.g., quantization origin/range/bits or signed-integer
  // minimum value offsets in k-d tree).
  size_t transform_data_bytes = 0;
  size_t total_attribute_bytes() const {
    return portable_attribute_bytes + transform_data_bytes;
  }
};

// Container and attribute byte accounting for a single
// PointCloudEncoder::Encode invocation.
//
// Invariant for all encoders on successful Encode():
//   header_and_metadata_bytes + connectivity_bytes +
//   joint_portable_attributes_bytes +
//   sum(attributes[i].total_attribute_bytes()) == total_bytes
// where connectivity_bytes == 0 for point-cloud encoders.
struct PointCloudEncodeStats {
  size_t total_bytes = 0;
  // Bytes written for the Draco header, geometry/attribute metadata, encoder
  // method/initialization headers, point-cloud point count header, and
  // attribute-encoder descriptor headers.
  size_t header_and_metadata_bytes = 0;
  // Bytes written for mesh connectivity when encoding triangular meshes; always
  // 0 for point clouds.
  size_t connectivity_bytes = 0;
  // Bytes written for jointly encoded portable attributes (e.g., k-d tree
  // point-cloud encoding where all attribute dimensions are interleaved in a
  // single tree bitstream); 0 when per_attribute_portable_bytes_exact == true.
  size_t joint_portable_attributes_bytes = 0;
  // True when each attribute's portable payload is encoded separately and
  // attributed to AttributeEncodeStats::portable_attribute_bytes.
  bool per_attribute_portable_bytes_exact = false;
  std::vector<AttributeEncodeStats> attributes;

  // Resets all scalar counters to zero/false and clears the attribute vector
  // while retaining its allocated capacity across repeated encodes.
  void Clear() {
    total_bytes = 0;
    header_and_metadata_bytes = 0;
    connectivity_bytes = 0;
    joint_portable_attributes_bytes = 0;
    per_attribute_portable_bytes_exact = false;
    attributes.clear();
  }
};

}  // namespace draco

#endif  // DRACO_COMPRESSION_POINT_CLOUD_POINT_CLOUD_ENCODE_STATS_H_
