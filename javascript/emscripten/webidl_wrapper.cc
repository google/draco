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
#include "javascript/emscripten/webidl_wrapper.h"

#include "compression/decode.h"
#include "mesh/mesh.h"

namespace draco {

DracoFloat32Array::DracoFloat32Array() {}

float DracoFloat32Array::GetValue(int index) const { return values_[index]; }

bool DracoFloat32Array::SetValues(const float *values, int count) {
  if (values) {
    values_.assign(values, values + count);
  } else {
    values_.resize(count);
  }
  return true;
}

DracoInt32Array::DracoInt32Array() {}

int DracoInt32Array::GetValue(int index) const { return values_[index]; }

bool DracoInt32Array::SetValues(const int *values, int count) {
  values_.assign(values, values + count);
  return true;
}

WebIDLWrapper::WebIDLWrapper() {}

draco_EncodedGeometryType WebIDLWrapper::GetEncodedGeometryType(
    DecoderBuffer *in_buffer) {
  return draco::GetEncodedGeometryType(in_buffer);
}

PointCloud *WebIDLWrapper::DecodePointCloudFromBuffer(
    DecoderBuffer *in_buffer) {
  std::unique_ptr<PointCloud> pc(draco::DecodePointCloudFromBuffer(in_buffer));
  return pc.release();
}

Mesh *WebIDLWrapper::DecodeMeshFromBuffer(DecoderBuffer *in_buffer) {
  std::unique_ptr<Mesh> mesh(draco::DecodeMeshFromBuffer(in_buffer));
  return mesh.release();
}

long WebIDLWrapper::GetAttributeId(const PointCloud &pc,
                                   draco_GeometryAttribute_Type type) {
  return pc.GetNamedAttributeId(type);
}

const PointAttribute *WebIDLWrapper::GetAttribute(const PointCloud &pc,
                                                  long att_id) {
  return pc.attribute(att_id);
}

bool WebIDLWrapper::GetFaceFromMesh(const Mesh &m, FaceIndex::ValueType face_id,
                                    DracoInt32Array *out_values) {
  const Mesh::Face &face = m.face(FaceIndex(face_id));
  return out_values->SetValues(reinterpret_cast<const int *>(face.data()),
                               face.size());
}

bool WebIDLWrapper::GetAttributeFloat(const PointAttribute &pa,
                                      AttributeValueIndex::ValueType val_index,
                                      DracoFloat32Array *out_values) {
  const int kMaxAttributeFloatValues = 4;
  const int components = pa.components_count();
  float values[kMaxAttributeFloatValues] = {-2.0, -2.0, -2.0, -2.0};
  if (!pa.ConvertValue<float>(AttributeValueIndex(val_index), values))
    return false;
  return out_values->SetValues(values, components);
}

bool WebIDLWrapper::GetAttributeFloatForAllPoints(
    const PointCloud &pc, const PointAttribute &pa,
    DracoFloat32Array *out_values) {
  const int components = pa.components_count();
  const int num_points = pc.num_points();
  const int num_entries = num_points * components;
  const int kMaxAttributeFloatValues = 4;
  float values[kMaxAttributeFloatValues] = {-2.0, -2.0, -2.0, -2.0};
  int entry_id = 0;

  out_values->SetValues(nullptr, num_entries);
  for (PointIndex i(0); i < num_points; ++i) {
    const AttributeValueIndex val_index = pa.mapped_index(i);
    if (!pa.ConvertValue<float>(val_index, values))
      return false;
    for (int j = 0; j < components; ++j) {
      out_values->SetValue(entry_id++, values[j]);
    }
  }
  return true;
}

}  // namespace draco
