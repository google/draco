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
#include "draco/compression/attributes/kd_tree_attributes_encoder.h"
#include "draco/compression/attributes/kd_tree_attributes_shared.h"
#include "draco/compression/point_cloud/algorithms/dynamic_integer_points_kd_tree_encoder.h"
#include "draco/compression/point_cloud/algorithms/float_points_tree_encoder.h"
#include "draco/compression/point_cloud/point_cloud_encoder.h"

namespace draco {

// Input random access iterator that is used to access the attribute values
// by the KdTree core classes.
template <class CoeffT, int dimension_t>
class PointAttributeVectorIterator {
  typedef PointAttributeVectorIterator<CoeffT, dimension_t> Self;

 public:
  typedef PointIndex::ValueType difference_type;
  typedef VectorD<CoeffT, dimension_t> value_type;
  typedef VectorD<CoeffT, dimension_t> *pointer;
  typedef VectorD<CoeffT, dimension_t> &reference;
  typedef std::random_access_iterator_tag iterator_category;

  explicit PointAttributeVectorIterator(const PointAttribute *att)
      : attribute_(att), point_id_(0) {}
  value_type operator*() const {
    value_type ret;
    attribute_->ConvertValue<CoeffT, dimension_t>(
        attribute_->mapped_index(point_id_), &ret[0]);
    return ret;
  }
  const Self &operator++() {
    ++point_id_;
    return *this;
  }
  bool operator!=(const Self &other) const {
    return point_id_ != other.point_id_;
  }
  PointIndex::ValueType operator-(const Self &i) const {
    return (point_id_ - i.point_id_).value();
  }
  Self operator+(PointIndex::ValueType dif) const {
    Self ret = *this;
    ret.point_id_ += dif;
    return ret;
  }
  bool operator<(const Self &other) const {
    return point_id_ < other.point_id_;
  }

 private:
  const PointAttribute *attribute_;
  PointIndex point_id_;
};

KdTreeAttributesEncoder::KdTreeAttributesEncoder() {}

KdTreeAttributesEncoder::KdTreeAttributesEncoder(int att_id)
    : AttributesEncoder(att_id) {}

bool KdTreeAttributesEncoder::EncodePortableAttributes(
    EncoderBuffer *out_buffer) {
  // Everything is currently encoded in the EncodeTransformData method.
  return true;
}

bool KdTreeAttributesEncoder::EncodeDataNeededByPortableTransforms(
    EncoderBuffer *out_buffer) {
  // TODO(ostava): This should be separated into two parts, lossy and lossless.
  // At this point we support only one attribute for this attributes encoder.
  if (num_attributes() != 1)
    return false;
  // Get the first attribute (which should be position).
  const int att_id = GetAttributeId(0);
  const PointAttribute *const att = encoder()->point_cloud()->attribute(att_id);
  if (att->num_components() != 3)
    return false;
  const uint8_t compression_level =
      std::min(10 - encoder()->options()->GetSpeed(), 6);
  DCHECK_LE(compression_level, 6);
  if (att->data_type() == DT_FLOAT32) {
    const int quantization_bits =
        encoder()->options()->GetAttributeInt(att_id, "quantization_bits", -1);
    if (quantization_bits <= 0) {
      // Algorithm works only for quantized points.
      return false;
    }
    out_buffer->Encode(static_cast<uint8_t>(
        KdTreeAttributesEncodingMethod::kKdTreeQuantizationEncoding));
    out_buffer->Encode(compression_level);
    out_buffer->Encode(
        static_cast<uint32_t>(encoder()->point_cloud()->num_points()));
    typedef PointAttributeVectorIterator<float, 3> AttributeIterator;
    FloatPointsTreeEncoder points_encoder(KDTREE, quantization_bits,
                                          compression_level);
    if (!points_encoder.EncodePointCloud(
            AttributeIterator(att),
            AttributeIterator(att) + encoder()->point_cloud()->num_points()))
      return false;
    out_buffer->Encode(points_encoder.buffer()->data(),
                       points_encoder.buffer()->size());
  } else if (att->data_type() == DT_UINT32) {
    out_buffer->Encode(static_cast<uint8_t>(
        KdTreeAttributesEncodingMethod::kKdTreeIntegerEncoding));
    out_buffer->Encode(compression_level);
    out_buffer->Encode(
        static_cast<uint32_t>(encoder()->point_cloud()->num_points()));
    typedef PointAttributeVectorIterator<uint32_t, 3> AttributeIterator;
    // For the integer points encoder, we need to first store the attribute
    // values in a vector because the encoder modifies the input container,
    // which is currently not acceptable for the input PointAttribute.
    AttributeIterator it(att);
    std::vector<Point3ui> int_points(
        it, it + encoder()->point_cloud()->num_points());

    switch (compression_level) {
      case 6: {
        DynamicIntegerPointsKdTreeEncoder<6> points_encoder(3);
        if (!points_encoder.EncodePoints(int_points.begin(), int_points.end(),
                                         out_buffer))
          return false;
        break;
      }
      case 5: {
        DynamicIntegerPointsKdTreeEncoder<5> points_encoder(3);
        if (!points_encoder.EncodePoints(int_points.begin(), int_points.end(),
                                         out_buffer))
          return false;
        break;
      }
      case 4: {
        DynamicIntegerPointsKdTreeEncoder<4> points_encoder(3);
        if (!points_encoder.EncodePoints(int_points.begin(), int_points.end(),
                                         out_buffer))
          return false;
        break;
      }
      case 3: {
        DynamicIntegerPointsKdTreeEncoder<3> points_encoder(3);
        if (!points_encoder.EncodePoints(int_points.begin(), int_points.end(),
                                         out_buffer))
          return false;
        break;
      }
      case 2: {
        DynamicIntegerPointsKdTreeEncoder<2> points_encoder(3);
        if (!points_encoder.EncodePoints(int_points.begin(), int_points.end(),
                                         out_buffer))
          return false;
        break;
      }
      case 1: {
        DynamicIntegerPointsKdTreeEncoder<1> points_encoder(3);
        if (!points_encoder.EncodePoints(int_points.begin(), int_points.end(),
                                         out_buffer))
          return false;
        break;
      }
      case 0: {
        DynamicIntegerPointsKdTreeEncoder<0> points_encoder(3);
        if (!points_encoder.EncodePoints(int_points.begin(), int_points.end(),
                                         out_buffer))
          return false;
        break;
      }
      // Compression level and/or encoding speed seem wrong.
      default:
        return false;
    }
  } else {
    // Unsupported data type.
    return false;
  }
  return true;
}

}  // namespace draco
