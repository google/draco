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
#include "compression/attributes/kd_tree_attributes_decoder.h"
#include "compression/attributes/kd_tree_attributes_shared.h"
#include "compression/point_cloud/algorithms/float_points_kd_tree_decoder.h"
#include "compression/point_cloud/algorithms/integer_points_kd_tree_decoder.h"
#include "compression/point_cloud/point_cloud_decoder.h"

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
  const Self &operator=(const VectorD<CoeffT, dimension_t> &val) {
    attribute_->SetAttributeValue(attribute_->mapped_index(point_id_), &val[0]);
    return *this;
  }

 private:
  PointAttribute *attribute_;
  PointIndex point_id_;
};

KdTreeAttributesDecoder::KdTreeAttributesDecoder() {}

bool KdTreeAttributesDecoder::DecodeAttributes(DecoderBuffer *in_buffer) {
  const int att_id = GetAttributeId(0);
  PointAttribute *const att = decoder()->point_cloud()->attribute(att_id);
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
    FloatPointsKdTreeDecoder decoder;
    PointAttributeVectorOutputIterator<float, 3> out_it(att);
    if (!decoder.DecodePointCloud(in_buffer, out_it))
      return false;
  } else if (method == KdTreeAttributesEncodingMethod::kKdTreeIntegerEncoding) {
    uint8_t compression_level = 0;
    if (!in_buffer->Decode(&compression_level))
      return false;
    uint32_t num_points;
    if (!in_buffer->Decode(&num_points))
      return false;
    att->Reset(num_points);
    PointAttributeVectorOutputIterator<uint32_t, 3> out_it(att);
    switch (compression_level) {
      case 0: {
        IntegerPointsKdTreeDecoder<Point3ui, 0> decoder;
        if (!decoder.DecodePoints(in_buffer, out_it))
          return false;
        break;
      }
      case 1: {
        IntegerPointsKdTreeDecoder<Point3ui, 1> decoder;
        if (!decoder.DecodePoints(in_buffer, out_it))
          return false;
        break;
      }
      case 2: {
        IntegerPointsKdTreeDecoder<Point3ui, 2> decoder;
        if (!decoder.DecodePoints(in_buffer, out_it))
          return false;
        break;
      }
      case 3: {
        IntegerPointsKdTreeDecoder<Point3ui, 3> decoder;
        if (!decoder.DecodePoints(in_buffer, out_it))
          return false;
        break;
      }
      case 4: {
        IntegerPointsKdTreeDecoder<Point3ui, 4> decoder;
        if (!decoder.DecodePoints(in_buffer, out_it))
          return false;
        break;
      }
      case 5: {
        IntegerPointsKdTreeDecoder<Point3ui, 5> decoder;
        if (!decoder.DecodePoints(in_buffer, out_it))
          return false;
        break;
      }
      case 6: {
        IntegerPointsKdTreeDecoder<Point3ui, 6> decoder;
        if (!decoder.DecodePoints(in_buffer, out_it))
          return false;
        break;
      }
      case 7: {
        IntegerPointsKdTreeDecoder<Point3ui, 7> decoder;
        if (!decoder.DecodePoints(in_buffer, out_it))
          return false;
        break;
      }
      case 8: {
        IntegerPointsKdTreeDecoder<Point3ui, 8> decoder;
        if (!decoder.DecodePoints(in_buffer, out_it))
          return false;
        break;
      }
      case 9: {
        IntegerPointsKdTreeDecoder<Point3ui, 9> decoder;
        if (!decoder.DecodePoints(in_buffer, out_it))
          return false;
        break;
      }
      case 10: {
        IntegerPointsKdTreeDecoder<Point3ui, 10> decoder;
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
