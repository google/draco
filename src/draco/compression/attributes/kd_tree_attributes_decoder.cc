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
#include "draco/compression/attributes/kd_tree_attributes_decoder.h"
#include "draco/compression/attributes/kd_tree_attributes_shared.h"
#include "draco/compression/point_cloud/algorithms/dynamic_integer_points_kd_tree_decoder.h"
#include "draco/compression/point_cloud/algorithms/float_points_tree_decoder.h"
#include "draco/compression/point_cloud/point_cloud_decoder.h"

namespace draco {

// Output iterator that is used to decode values directly into the data buffer
// of the modified PointAttribute.
template <class CoeffT, int dimension_t>
class PointAttributeVectorOutputIterator {
  typedef PointAttributeVectorOutputIterator<CoeffT, dimension_t> Self;

 public:
  explicit PointAttributeVectorOutputIterator(PointAttribute *att)
      : attribute_(att), point_id_(0) {}

  const Self &operator++() {
    ++point_id_;
    return *this;
  }
  Self operator++(int) {
    Self copy = *this;
    ++point_id_;
    return copy;
  }
  Self &operator*() { return *this; }
  // Still needed in some cases.
  // TODO(hemmer): remove.
  const Self &operator=(const VectorD<CoeffT, dimension_t> &val) {
    attribute_->SetAttributeValue(attribute_->mapped_index(point_id_), &val[0]);
    return *this;
  }
  // Additional operator taking std::vector as argument.
  const Self &operator=(const std::vector<CoeffT> &val) {
    DCHECK_EQ(val.size(), dimension_t);
    attribute_->SetAttributeValue(attribute_->mapped_index(point_id_),
                                  val.data());
    return *this;
  }

 private:
  PointAttribute *attribute_;
  PointIndex point_id_;
};

KdTreeAttributesDecoder::KdTreeAttributesDecoder() {}

bool KdTreeAttributesDecoder::DecodePortableAttributes(
    DecoderBuffer *in_buffer) {
  // Everything is currently done in RevertTransform method, because it can
  // perform lossy operation on floating point data.
  // TODO(ostava): Refactor this to two separate steps (lossy + lossless).
  return true;
}

bool KdTreeAttributesDecoder::DecodeDataNeededByPortableTransforms(
    DecoderBuffer *in_buffer) {
  const int att_id = GetAttributeId(0);
  PointAttribute *const att = GetDecoder()->point_cloud()->attribute(att_id);
  att->SetIdentityMapping();
  // Decode method
  uint8_t method;
  if (!in_buffer->Decode(&method))
    return false;
  if (method == KdTreeAttributesEncodingMethod::kKdTreeQuantizationEncoding) {
    uint8_t compression_level = 0;
    if (!in_buffer->Decode(&compression_level))
      return false;
    uint32_t num_points = 0;
    if (!in_buffer->Decode(&num_points))
      return false;
    att->Reset(num_points);
    FloatPointsTreeDecoder decoder;
    PointAttributeVectorOutputIterator<float, 3> out_it(att);
    if (!decoder.DecodePointCloud(in_buffer, out_it))
      return false;
  } else if (method == KdTreeAttributesEncodingMethod::kKdTreeIntegerEncoding) {
    uint8_t compression_level = 0;
    if (!in_buffer->Decode(&compression_level))
      return false;
    if (6 < compression_level) {
      LOGE("KdTreeAttributesDecoder: compression level %i not supported.\n",
           compression_level);
      return false;
    }

    uint32_t num_points;
    if (!in_buffer->Decode(&num_points))
      return false;
    att->Reset(num_points);
    PointAttributeVectorOutputIterator<uint32_t, 3> out_it(att);

    switch (compression_level) {
      case 0: {
        DynamicIntegerPointsKdTreeDecoder<0> decoder(3);
        if (!decoder.DecodePoints(in_buffer, out_it))
          return false;
        break;
      }
      case 1: {
        DynamicIntegerPointsKdTreeDecoder<1> decoder(3);
        if (!decoder.DecodePoints(in_buffer, out_it))
          return false;
        break;
      }
      case 2: {
        DynamicIntegerPointsKdTreeDecoder<2> decoder(3);
        if (!decoder.DecodePoints(in_buffer, out_it))
          return false;
        break;
      }
      case 3: {
        DynamicIntegerPointsKdTreeDecoder<3> decoder(3);
        if (!decoder.DecodePoints(in_buffer, out_it))
          return false;
        break;
      }
      case 4: {
        DynamicIntegerPointsKdTreeDecoder<4> decoder(3);
        if (!decoder.DecodePoints(in_buffer, out_it))
          return false;
        break;
      }
      case 5: {
        DynamicIntegerPointsKdTreeDecoder<5> decoder(3);
        if (!decoder.DecodePoints(in_buffer, out_it))
          return false;
        break;
      }
      case 6: {
        DynamicIntegerPointsKdTreeDecoder<6> decoder(3);
        if (!decoder.DecodePoints(in_buffer, out_it))
          return false;
        break;
      }
      default:
        return false;
    }
  } else {
    // Invalid method.
    return false;
  }
  return true;
}

}  // namespace draco
