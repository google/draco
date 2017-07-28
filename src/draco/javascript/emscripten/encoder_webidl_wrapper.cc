// Copyright 2017 The Draco Authors.
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
#include "draco/javascript/emscripten/encoder_webidl_wrapper.h"

#include "draco/compression/encode.h"
#include "draco/mesh/mesh.h"

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

DracoInt8Array::DracoInt8Array() {}

int DracoInt8Array::GetValue(int index) const { return values_[index]; }

bool DracoInt8Array::SetValues(const char *values, int count) {
  values_.assign(values, values + count);
  return true;
}

using draco::Mesh;
using draco::Metadata;
using draco::PointCloud;

MetadataBuilder::MetadataBuilder() {}

bool MetadataBuilder::AddStringEntry(Metadata *metadata, const char *entry_name,
                                     const char *entry_value) {
  if (!metadata)
    return false;
  const std::string name{entry_name};
  const std::string value{entry_value};
  metadata->AddEntryString(entry_name, entry_value);
  return true;
}

bool MetadataBuilder::AddIntEntry(Metadata *metadata, const char *entry_name,
                                  long entry_value) {
  if (!metadata)
    return false;
  const std::string name{entry_name};
  metadata->AddEntryInt(name, entry_value);
  return true;
}

bool MetadataBuilder::AddDoubleEntry(Metadata *metadata, const char *entry_name,
                                     double entry_value) {
  if (!metadata)
    return false;
  const std::string name{entry_name};
  metadata->AddEntryDouble(name, entry_value);
  return true;
}

MeshBuilder::MeshBuilder() {}

bool MeshBuilder::AddFacesToMesh(Mesh *mesh, long num_faces, const int *faces) {
  if (!mesh)
    return false;
  mesh->SetNumFaces(num_faces);
  for (draco::FaceIndex i(0); i < num_faces; ++i) {
    draco::Mesh::Face face;
    face[0] = faces[i.value() * 3];
    face[1] = faces[i.value() * 3 + 1];
    face[2] = faces[i.value() * 3 + 2];
    mesh->SetFace(i, face);
  }
  return true;
}

int MeshBuilder::AddFloatAttributeToMesh(Mesh *mesh,
                                         draco_GeometryAttribute_Type type,
                                         long num_vertices, long num_components,
                                         const float *att_values) {
  if (!mesh)
    return -1;
  draco::PointAttribute att;
  att.Init(type, NULL, num_components, draco::DT_FLOAT32,
           /* normalized */ false,
           /* stride */ sizeof(float) * num_components, /* byte_offset */ 0);
  const int att_id =
      mesh->AddAttribute(att, /* identity_mapping */ true, num_vertices);
  draco::PointAttribute *const att_ptr = mesh->attribute(att_id);

  for (draco::PointIndex i(0); i < num_vertices; ++i) {
    att_ptr->SetAttributeValue(att_ptr->mapped_index(i),
                               &att_values[i.value() * num_components]);
  }
  if (mesh->num_points() == 0) {
    mesh->set_num_points(num_vertices);
  } else if (mesh->num_points() != num_vertices) {
    return -1;
  }
  return att_id;
}

bool MeshBuilder::SetMetadataForAttribute(Mesh *mesh, long attribute_id,
                                          const Metadata *metadata) {
  if (!mesh)
    return false;
  // If empty metadata, just ignore.
  if (!metadata)
    return false;
  if (attribute_id < 0)
    return false;
  if (!mesh->metadata()) {
    std::unique_ptr<draco::GeometryMetadata> geometry_metadata =
        std::unique_ptr<draco::GeometryMetadata>(new draco::GeometryMetadata());
    mesh->AddMetadata(std::move(geometry_metadata));
  }
  std::unique_ptr<draco::AttributeMetadata> att_metadata =
      std::unique_ptr<draco::AttributeMetadata>(
          new draco::AttributeMetadata(attribute_id, *metadata));
  mesh->metadata()->AddAttributeMetadata(std::move(att_metadata));
  return true;
}

bool MeshBuilder::AddMetadataToMesh(Mesh *mesh, const Metadata *metadata) {
  if (!mesh)
    return false;
  // Not allow write over metadata.
  if (mesh->metadata())
    return false;
  std::unique_ptr<draco::GeometryMetadata> new_metadata =
      std::unique_ptr<draco::GeometryMetadata>(
          new draco::GeometryMetadata(*metadata));
  mesh->AddMetadata(std::move(new_metadata));
  return true;
}

Encoder::Encoder() {}

void Encoder::SetEncodingMethod(draco_MeshEncoderMethod method) {
  encoder_.SetEncodingMethod(method);
}

void Encoder::SetAttributeQuantization(draco_GeometryAttribute_Type type,
                                       long quantization_bits) {
  encoder_.SetAttributeQuantization(type, quantization_bits);
}

void Encoder::SetSpeedOptions(long encoding_speed, long decoding_speed) {
  encoder_.SetSpeedOptions(encoding_speed, decoding_speed);
}

int Encoder::EncodeMeshToDracoBuffer(Mesh *mesh, DracoInt8Array *draco_buffer) {
  if (!mesh)
    return 0;
  draco::EncoderBuffer buffer;
  if (mesh->GetNamedAttributeId(draco::GeometryAttribute::POSITION) == -1)
    return 0;
  if (!mesh->DeduplicateAttributeValues())
    return 0;
  mesh->DeduplicatePointIds();
  if (!encoder_.EncodeMeshToBuffer(*mesh, &buffer).ok()) {
    return 0;
  }
  draco_buffer->SetValues(buffer.data(), buffer.size());
  return buffer.size();
}
