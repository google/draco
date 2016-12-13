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
#ifndef DRACO_POINT_CLOUD_GEOMETRY_ATTRIBUTE_H_
#define DRACO_POINT_CLOUD_GEOMETRY_ATTRIBUTE_H_

#include <array>
#include <limits>

#include "core/data_buffer.h"
#include "core/hash_utils.h"
#include "point_cloud/geometry_indices.h"

namespace draco {

// The class provides access to a specific attribute which is stored in a
// DataBuffer, such as normals or coordinates. However, the GeometryAttribute
// class does not own the buffer and the buffer itself may store other data
// unrelated to this attribute (such as data for other attributes in which case
// we can have multiple GeometryAttributes accessing one buffer). Typically,
// all attributes for a point (or corner, face) are stored in one block, which
// is advantageous in terms of memory access. The length of the entire block is
// given by the byte_stride, the position where the attribute starts is given by
// the byte_offset, the actual number of bytes that the attribute occupies is
// given by the data_type and the number of components.
class GeometryAttribute {
 public:
  // Supported attribute types.
  enum Type {
    INVALID = -1,
    // Named attributes start here. The difference between named and generic
    // attributes is that for named attributes we know their purpose and we
    // can apply some special methods when dealing with them (e.g. during
    // encoding).
    POSITION = 0,
    NORMAL,
    COLOR,
    TEX_COORD,
    // Total number of different attribute types.
    // Always keep behind all named attributes.
    NAMED_ATTRIBUTES_COUNT,
    // A special id used to mark attributes that are not assigned to any known
    // predefined use case. Such attributes are often used for a shader specific
    // data.
    GENERIC = NAMED_ATTRIBUTES_COUNT
  };

  GeometryAttribute();
  // Initializes and enables the attribute.
  void Init(Type attribute_type, const DataBuffer *buffer,
            int8_t components_count, DataType data_type, bool normalized,
            int64_t byte_stride, int64_t byte_offset);
  bool IsValid() const { return buffer_ != nullptr; }

  // Function for getting a attribute value with a specific format.
  // Unsafe. Caller must ensure the accessed memory is valid.
  // T is the attribute data type.
  // att_components_t is the number of attribute components.
  template <typename T, int att_components_t>
  std::array<T, att_components_t> GetValue(
      AttributeValueIndex att_index) const {
    // Byte address of the attribute index.
    const int64_t byte_pos = byte_offset_ + byte_stride_ * att_index.value();
    std::array<T, att_components_t> out;
    buffer_->Read(byte_pos, &(out[0]), sizeof(out));
    return out;
  }

  // Returns the byte position of the attribute entry in the data buffer.
  inline int64_t GetBytePos(AttributeValueIndex att_index) const {
    return byte_offset_ + byte_stride_ * att_index.value();
  }

  inline const uint8_t *GetAddress(AttributeValueIndex att_index) const {
    const int64_t byte_pos = GetBytePos(att_index);
    return buffer_->data() + byte_pos;
  }

  // Fills out_data with the raw value of the requested attribute entry.
  // out_data must be at least byte_stride_ long.
  void GetValue(AttributeValueIndex att_index, void *out_data) const {
    const int64_t byte_pos = byte_offset_ + byte_stride_ * att_index.value();
    buffer_->Read(byte_pos, out_data, byte_stride_);
  }

  // Function for conversion of a attribute to a specific output format.
  // OutT is the desired data type of the attribute.
  // out_att_components_t is the number of components of the output format.
  // Returns false when the conversion failed.
  template <typename OutT, int out_att_components_t>
  bool ConvertValue(AttributeValueIndex att_id, OutT *out_val) const {
    if (out_val == nullptr)
      return false;
    switch (data_type_) {
      case DT_FLOAT32:
        return ConvertTypedValue<float, OutT, out_att_components_t>(att_id,
                                                                    out_val);
      case DT_UINT8:
        return ConvertTypedValue<uint8_t, OutT, out_att_components_t>(att_id,
                                                                      out_val);
      case DT_INT8:
        return ConvertTypedValue<int8_t, OutT, out_att_components_t>(att_id,
                                                                     out_val);
      case DT_UINT16:
        return ConvertTypedValue<uint16_t, OutT, out_att_components_t>(att_id,
                                                                       out_val);
      case DT_INT16:
        return ConvertTypedValue<int16_t, OutT, out_att_components_t>(att_id,
                                                                      out_val);
      case DT_UINT32:
        return ConvertTypedValue<uint32_t, OutT, out_att_components_t>(att_id,
                                                                       out_val);
      case DT_INT32:
        return ConvertTypedValue<int32_t, OutT, out_att_components_t>(att_id,
                                                                      out_val);
      default:
        // Wrong attribute type.
        return false;
    }
  }

  // Function for conversion of a attribute to a specific output format.
  // The |out_value| must be able to store all components of a single attribute
  // entry.
  // OutT is the desired data type of the attribute.
  // Returns false when the conversion failed.
  template <typename OutT>
  bool ConvertValue(AttributeValueIndex att_index, OutT *out_value) const {
    switch (components_count_) {
      case 1:
        return ConvertValue<OutT, 1>(att_index, out_value);
      case 2:
        return ConvertValue<OutT, 2>(att_index, out_value);
      case 3:
        return ConvertValue<OutT, 3>(att_index, out_value);
      case 4:
        return ConvertValue<OutT, 4>(att_index, out_value);
      default:
        break;
    }
    return false;
  }

  bool operator==(const GeometryAttribute &va) const;

  // Returns the type of the attribute indicating the nature of the attribute.
  Type attribute_type() const { return attribute_type_; }
  void set_attribute_type(Type type) { attribute_type_ = type; }
  // Retruns the data type that is stored in the attrbute.
  DataType data_type() const { return data_type_; }
  // Returns the number of components that are stored for each entry.
  // For position attrinute this is usually three (x,y,z),
  // while texture coordinates have two compontents (u,v).
  int8_t components_count() const { return components_count_; }
  // Indicates whether the data type should be normalized before interpretation,
  // that is, it should be devided by the max value of the data type.
  bool normalized() const { return normalized_; }
  // The buffer storing the entire data of the attribute.
  const DataBuffer *buffer() const { return buffer_; }
  // Returns the number of bytes between two attribute entries, this is, at
  // least size of the data types times number of components.
  int64_t byte_stride() const { return byte_stride_; }
  // The offset where the attribute starts within the block of size byte_stride.
  int64_t byte_offset() const { return byte_offset_; }
  void set_byte_offset(int64_t byte_offset) { byte_offset_ = byte_offset; }
  DataBufferDescriptor buffer_descriptor() const { return buffer_descriptor_; }
  uint16_t custom_id() const { return custom_id_; }
  void set_custom_id(uint16_t id) { custom_id_ = id; }

 protected:
  // Sets a new internal storage for the attribute.
  void ResetBuffer(const DataBuffer *buffer, int64_t byte_stride,
                   int64_t byte_offset);

 private:
  // Function for conversion of an attribute to a specific output format given a
  // format of the stored attribute.
  // T is the stored attribute data type.
  // att_components_t is the number of the stored attribute components.
  // OutT is the desired data type of the attribute.
  // out_att_components_t is the number of components of the output format.
  template <typename T, int att_components_t, typename OutT,
            int out_att_components_t>
  void ConvertTypedValue(AttributeValueIndex att_id, OutT *out_value) const {
    std::array<T, att_components_t> att_value =
        GetValue<T, att_components_t>(att_id);
    // Convert all components available in both the original and output formats.
    for (int i = 0; i < std::min(att_components_t, out_att_components_t); ++i) {
      out_value[i] = static_cast<OutT>(att_value[i]);
      // When converting integer to floating point, normalize the value if
      // necessary.
      if (std::is_integral<T>::value && std::is_floating_point<OutT>::value &&
          normalized_) {
        out_value[i] /= static_cast<OutT>(std::numeric_limits<T>::max());
      }
      // TODO(ostava): Add handling of normalized attributes when converting
      // between different integer representations. If the attribute is
      // normalized, integer values should be converted as if they represent 0-1
      // range. E.g. when we convert uint16 to uint8, the range <0, 2^16 - 1>
      // should be converted to range <0, 2^8 - 1>.
    }
    // Fill empty data for unused output components if needed.
    for (int i = att_components_t; i < out_att_components_t; ++i) {
      out_value[i] = static_cast<OutT>(0);
    }
  }

  // The same as above but without a component specifier for input attribute.
  template <typename T, typename OutT, int OUT_CMPS>
  bool ConvertTypedValue(AttributeValueIndex att_index, OutT *out_value) const {
    // Selecte the right method to call based on the number of attribute
    // components.
    switch (components_count_) {
      case 1:
        ConvertTypedValue<T, 1, OutT, OUT_CMPS>(att_index, out_value);
        break;
      case 2:
        ConvertTypedValue<T, 2, OutT, OUT_CMPS>(att_index, out_value);
        break;
      case 3:
        ConvertTypedValue<T, 3, OutT, OUT_CMPS>(att_index, out_value);
        break;
      case 4:
        ConvertTypedValue<T, 4, OutT, OUT_CMPS>(att_index, out_value);
        break;
      default:
        return false;  // This shouldn't happen.
    }
    return true;
  }

  const DataBuffer *buffer_;
  // The buffer descriptor is stored at the time the buffer is attached to this
  // attribute. The purpose is to detect if any changes happened to the buffer
  // since the time it was attached.
  DataBufferDescriptor buffer_descriptor_;
  int8_t components_count_;
  DataType data_type_;
  bool normalized_;
  int64_t byte_stride_;
  int64_t byte_offset_;

  Type attribute_type_;

  // User defined 16-bit id that can be for example used to identify generic
  // attributes. By default |custom_id_| == 0.
  uint16_t custom_id_;

  friend struct GeometryAttributeHasher;
};

// Hashing support

// Function object for using Attribute as a hash key.
struct GeometryAttributeHasher {
  size_t operator()(const GeometryAttribute &va) const {
    size_t hash = HashCombine(va.buffer_descriptor_.buffer_id,
                              va.buffer_descriptor_.buffer_update_count);
    hash = HashCombine(va.components_count_, hash);
    hash = HashCombine((int8_t)va.data_type_, hash);
    hash = HashCombine((int8_t)va.attribute_type_, hash);
    hash = HashCombine(va.byte_stride_, hash);
    return HashCombine(va.byte_offset_, hash);
  }
};

// Function object for using GeometryAttribute::Type as a hash key.
struct GeometryAttributeTypeHasher {
  size_t operator()(const GeometryAttribute::Type &at) const {
    return static_cast<size_t>(at);
  }
};

}  // namespace draco

#endif  // DRACO_POINT_CLOUD_GEOMETRY_ATTRIBUTE_H_
