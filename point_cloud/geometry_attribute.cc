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
#include "point_cloud/geometry_attribute.h"

using std::array;

namespace draco {

GeometryAttribute::GeometryAttribute()
    : buffer_(nullptr),
      components_count_(1),
      data_type_(DT_FLOAT32),
      byte_stride_(0),
      byte_offset_(0),
      attribute_type_(INVALID),
      custom_id_(0) {}

void GeometryAttribute::Init(GeometryAttribute::Type attribute_type,
                             const DataBuffer *buffer, int8_t components_count,
                             DataType data_type, bool normalized,
                             int64_t byte_stride, int64_t byte_offset) {
  buffer_ = buffer;
  if (buffer) {
    buffer_descriptor_.buffer_id = buffer->buffer_id();
    buffer_descriptor_.buffer_update_count = buffer->update_count();
  }
  components_count_ = components_count;
  data_type_ = data_type;
  normalized_ = normalized;
  byte_stride_ = byte_stride;
  byte_offset_ = byte_offset;
  attribute_type_ = attribute_type;
}

bool GeometryAttribute::operator==(const GeometryAttribute &va) const {
  if (attribute_type_ != va.attribute_type_)
    return false;
  // It's ok to compare just the buffer descriptors here. We don't need to
  // compare the buffers themselves.
  if (buffer_descriptor_.buffer_id != va.buffer_descriptor_.buffer_id)
    return false;
  if (buffer_descriptor_.buffer_update_count !=
      va.buffer_descriptor_.buffer_update_count)
    return false;
  if (components_count_ != va.components_count_)
    return false;
  if (data_type_ != va.data_type_)
    return false;
  if (byte_stride_ != va.byte_stride_)
    return false;
  if (byte_offset_ != va.byte_offset_)
    return false;
  return true;
}

void GeometryAttribute::ResetBuffer(const DataBuffer *buffer,
                                    int64_t byte_stride, int64_t byte_offset) {
  buffer_ = buffer;
  buffer_descriptor_.buffer_id = buffer->buffer_id();
  buffer_descriptor_.buffer_update_count = buffer->update_count();
  byte_stride_ = byte_stride;
  byte_offset_ = byte_offset;
}

}  // namespace draco
