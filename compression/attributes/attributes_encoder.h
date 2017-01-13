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
#ifndef DRACO_COMPRESSION_ATTRIBUTES_ATTRIBUTES_ENCODER_H_
#define DRACO_COMPRESSION_ATTRIBUTES_ATTRIBUTES_ENCODER_H_

#include "core/encoder_buffer.h"
#include "point_cloud/point_attribute.h"
#include "point_cloud/point_cloud.h"

namespace draco {

class PointCloudEncoder;

// Base class for encoding one or more attributes of a PointCloud (or other
// geometry). This base class provides only the basic interface that is used
// by the PointCloudEncoder. The actual encoding must be implemented in derived
// classes using the EncodeAttributes() method.
class AttributesEncoder {
 public:
  AttributesEncoder();
  // Constructs an attribute encoder assosciated with a given point attribute.
  explicit AttributesEncoder(int point_attrib_id);
  virtual ~AttributesEncoder() = default;

  // Called after all attribute encoders are created. It can be used to perform
  // any custom initialization, including setting up attribute dependencies.
  // Note: no data should be encoded in this function, because the decoder may
  // process encoders in a different order from the decoder.
  virtual bool Initialize(PointCloudEncoder *encoder, const PointCloud *pc);

  // Encodes data needed by the target attribute decoder.
  virtual bool EncodeAttributesEncoderData(EncoderBuffer *out_buffer);

  // Returns a unique identifier of the given encoder type, that is used during
  // decoding to construct the corresponding attribute decoder.
  virtual uint8_t GetUniqueId() const = 0;

  // Encode attribute data to the target buffer. Needs to be implmented by the
  // derived classes.
  virtual bool EncodeAttributes(EncoderBuffer *out_buffer) = 0;

  // Returns the number of attributes that need to be encoded before the
  // specified attribute is encoded.
  // Note that the attribute is specified by its point attribute id.
  virtual int NumParentAttributes(int32_t /* point_attribute_id */) const {
    return 0;
  }

  virtual int GetParentAttributeId(int32_t /* point_attribute_id */,
                                   int32_t /* parent_i */) const {
    return -1;
  }

  // Marks a given attribute as a parent of another attribute.
  virtual bool MarkParentAttribute(int32_t /* point_attribute_id */) {
    return false;
  }

  // Returns an attribute containing the encoded version of the attribute data.
  // I.e., the data that is going to be used by the decoder after the attribute
  // is decoded.
  // This is the same data as the original attribute for lossless encoders, but
  // the data can be different for lossy encoders. This data can be used by any
  // dependent attributes that require to use the same data that will be
  // availalbe during decoding.
  virtual const PointAttribute *GetLossyAttributeData(
      int32_t /* point_attribute_id */) {
    return nullptr;
  }

  void AddAttributeId(int32_t id) {
    point_attribute_ids_.push_back(id);
    if (id >= static_cast<int32_t>(point_attribute_to_local_id_map_.size()))
      point_attribute_to_local_id_map_.resize(id + 1, -1);
    point_attribute_to_local_id_map_[id] = point_attribute_ids_.size() - 1;
  }

  // Sets new attribute point ids (replacing the existing ones).
  void SetAttributeIds(const std::vector<int32_t> &point_attribute_ids) {
    point_attribute_ids_.clear();
    point_attribute_to_local_id_map_.clear();
    for (int32_t att_id : point_attribute_ids) {
      AddAttributeId(att_id);
    }
  }

  int32_t GetAttributeId(int i) const { return point_attribute_ids_[i]; }
  int32_t num_attributes() const { return point_attribute_ids_.size(); }
  PointCloudEncoder *encoder() const { return point_cloud_encoder_; }

 protected:
  int32_t GetLocalIdForPointAttribute(int32_t point_attribute_id) const {
    const int id_map_size = point_attribute_to_local_id_map_.size();
    if (point_attribute_id >= id_map_size)
      return -1;
    return point_attribute_to_local_id_map_[point_attribute_id];
  }

 private:
  // List of attribute ids that need to be encoded with this encoder.
  std::vector<int32_t> point_attribute_ids_;

  // Map between point attribute id and the local id (i.e., the inverse of the
  // |point_attribute_ids_|.
  std::vector<int32_t> point_attribute_to_local_id_map_;

  PointCloudEncoder *point_cloud_encoder_;
  const PointCloud *point_cloud_;
};

}  // namespace draco

#endif  // DRACO_COMPRESSION_ATTRIBUTES_ATTRIBUTES_ENCODER_H_
