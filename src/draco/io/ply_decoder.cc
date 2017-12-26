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
#include "draco/io/ply_decoder.h"

#include <fstream>

#include "draco/core/macros.h"
#include "draco/io/ply_property_reader.h"

namespace draco {

PlyDecoder::PlyDecoder() : out_mesh_(nullptr), out_point_cloud_(nullptr) {}

bool PlyDecoder::DecodeFromFile(const std::string &file_name, Mesh *out_mesh) {
  out_mesh_ = out_mesh;
  return DecodeFromFile(file_name, static_cast<PointCloud *>(out_mesh));
}

bool PlyDecoder::DecodeFromFile(const std::string &file_name,
                                PointCloud *out_point_cloud) {
  std::ifstream file(file_name, std::ios::binary);
  if (!file)
    return false;
  // Read the whole file into a buffer.
  auto pos0 = file.tellg();
  file.seekg(0, std::ios::end);
  auto file_size = file.tellg() - pos0;
  if (file_size == 0)
    return false;
  file.seekg(0, std::ios::beg);
  std::vector<char> data(file_size);
  file.read(&data[0], file_size);

  buffer_.Init(&data[0], file_size);
  return DecodeFromBuffer(&buffer_, out_point_cloud);
}

bool PlyDecoder::DecodeFromBuffer(DecoderBuffer *buffer, Mesh *out_mesh) {
  out_mesh_ = out_mesh;
  return DecodeFromBuffer(buffer, static_cast<PointCloud *>(out_mesh));
}

bool PlyDecoder::DecodeFromBuffer(DecoderBuffer *buffer,
                                  PointCloud *out_point_cloud) {
  out_point_cloud_ = out_point_cloud;
  buffer_.Init(buffer->data_head(), buffer->remaining_size());
  return DecodeInternal();
}

bool PlyDecoder::DecodeInternal() {
  PlyReader ply_reader;
  if (!ply_reader.Read(buffer()))
    return false;
  // First, decode the connectivity data.
  if (out_mesh_ && !DecodeFaceData(ply_reader.GetElementByName("face")))
    return false;
  // Decode all attributes.
  if (!DecodeVertexData(ply_reader.GetElementByName("vertex")))
    return false;
#ifdef DRACO_ATTRIBUTE_DEDUPLICATION_SUPPORTED
  // In case there are no faces this is just a point cloud which does
  // not require deduplication.
  if (out_mesh_ && out_mesh_->num_faces() != 0) {
    if (!out_point_cloud_->DeduplicateAttributeValues())
      return false;
    out_point_cloud_->DeduplicatePointIds();
  }
#endif
  return true;
}

bool PlyDecoder::DecodeFaceData(const PlyElement *face_element) {
  // We accept point clouds now.
  if (face_element == nullptr) {
    return true;
  }
  const int64_t num_faces = face_element->num_entries();
  out_mesh_->SetNumFaces(num_faces);
  const PlyProperty *vertex_indices =
      face_element->GetPropertyByName("vertex_indices");
  if (vertex_indices == nullptr) {
    // The property name may be named either "vertex_indices" or "vertex_index".
    vertex_indices = face_element->GetPropertyByName("vertex_index");
  }
  if (vertex_indices == nullptr || !vertex_indices->is_list()) {
    return false;  // No faces defined.
  }

  PlyPropertyReader<PointIndex::ValueType> vertex_index_reader(vertex_indices);
  Mesh::Face face;
  FaceIndex face_index(0);
  for (int i = 0; i < num_faces; ++i) {
    const int64_t list_offset = vertex_indices->GetListEntryOffset(i);
    const int64_t list_size = vertex_indices->GetListEntryNumValues(i);
    // TODO(ostava): Assume triangular faces only for now.
    if (list_size != 3)
      continue;  // All non-triangular faces are skipped.
    for (int64_t c = 0; c < 3; ++c)
      face[c] = vertex_index_reader.ReadValue(list_offset + c);
    out_mesh_->SetFace(face_index, face);
    face_index++;
  }
  out_mesh_->SetNumFaces(face_index.value());
  return true;
}

bool PlyDecoder::DecodeVertexData(const PlyElement *vertex_element) {
  if (vertex_element == nullptr)
    return false;
  // TODO(ostava): For now, try to load x,y,z vertices and red,green,blue,alpha
  // colors. We need to add other properties later.
  const PlyProperty *const x_prop = vertex_element->GetPropertyByName("x");
  const PlyProperty *const y_prop = vertex_element->GetPropertyByName("y");
  const PlyProperty *const z_prop = vertex_element->GetPropertyByName("z");
  if (!x_prop || !y_prop || !z_prop) {
    // Currently, we require 3 vertex coordinates (this should be generalized
    // later on).
    return false;
  }
  const PointIndex::ValueType num_vertices = vertex_element->num_entries();
  out_point_cloud_->set_num_points(num_vertices);
  // Decode vertex positions.
  {
    // TODO(ostava): For now assume the position types are float32.
    if (x_prop->data_type() != DT_FLOAT32 ||
        y_prop->data_type() != DT_FLOAT32 ||
        z_prop->data_type() != DT_FLOAT32) {
      return false;
    }
    PlyPropertyReader<float> x_reader(x_prop);
    PlyPropertyReader<float> y_reader(y_prop);
    PlyPropertyReader<float> z_reader(z_prop);
    GeometryAttribute va;
    va.Init(GeometryAttribute::POSITION, nullptr, 3, DT_FLOAT32, false,
            sizeof(float) * 3, 0);
    const int att_id = out_point_cloud_->AddAttribute(va, true, num_vertices);
    for (PointIndex::ValueType i = 0; i < num_vertices; ++i) {
      std::array<float, 3> val;
      val[0] = x_reader.ReadValue(i);
      val[1] = y_reader.ReadValue(i);
      val[2] = z_reader.ReadValue(i);
      out_point_cloud_->attribute(att_id)->SetAttributeValue(
          AttributeValueIndex(i), &val[0]);
    }
  }

  // Decode normals if present.
  const PlyProperty *const n_x_prop = vertex_element->GetPropertyByName("nx");
  const PlyProperty *const n_y_prop = vertex_element->GetPropertyByName("ny");
  const PlyProperty *const n_z_prop = vertex_element->GetPropertyByName("nz");
  if (n_x_prop != nullptr && n_y_prop != nullptr && n_z_prop != nullptr) {
    // For now, all normal properties must be set and of type float32
    if (n_x_prop->data_type() == DT_FLOAT32 &&
        n_y_prop->data_type() == DT_FLOAT32 &&
        n_z_prop->data_type() == DT_FLOAT32) {
      PlyPropertyReader<float> x_reader(n_x_prop);
      PlyPropertyReader<float> y_reader(n_y_prop);
      PlyPropertyReader<float> z_reader(n_z_prop);
      GeometryAttribute va;
      va.Init(GeometryAttribute::NORMAL, nullptr, 3, DT_FLOAT32, false,
              sizeof(float) * 3, 0);
      const int att_id = out_point_cloud_->AddAttribute(va, true, num_vertices);
      for (PointIndex::ValueType i = 0; i < num_vertices; ++i) {
        std::array<float, 3> val;
        val[0] = x_reader.ReadValue(i);
        val[1] = y_reader.ReadValue(i);
        val[2] = z_reader.ReadValue(i);
        out_point_cloud_->attribute(att_id)->SetAttributeValue(
            AttributeValueIndex(i), &val[0]);
      }
    }
  }

  // Decode color data if present.
  int num_colors = 0;
  const PlyProperty *const r_prop = vertex_element->GetPropertyByName("red");
  const PlyProperty *const g_prop = vertex_element->GetPropertyByName("green");
  const PlyProperty *const b_prop = vertex_element->GetPropertyByName("blue");
  const PlyProperty *const a_prop = vertex_element->GetPropertyByName("alpha");
  if (r_prop)
    ++num_colors;
  if (g_prop)
    ++num_colors;
  if (b_prop)
    ++num_colors;
  if (a_prop)
    ++num_colors;

  if (num_colors) {
    std::vector<std::unique_ptr<PlyPropertyReader<uint8_t>>> color_readers;
    const PlyProperty *p;
    if (r_prop) {
      p = r_prop;
      // TODO(ostava): For now ensure the data type of all components is uint8.
      DCHECK_EQ(true, p->data_type() == DT_UINT8);
      if (p->data_type() != DT_UINT8)
        return false;
      color_readers.push_back(std::unique_ptr<PlyPropertyReader<uint8_t>>(
          new PlyPropertyReader<uint8_t>(p)));
    }
    if (g_prop) {
      p = g_prop;
      // TODO(ostava): For now ensure the data type of all components is uint8.
      DCHECK_EQ(true, p->data_type() == DT_UINT8);
      if (p->data_type() != DT_UINT8)
        return false;
      color_readers.push_back(std::unique_ptr<PlyPropertyReader<uint8_t>>(
          new PlyPropertyReader<uint8_t>(p)));
    }
    if (b_prop) {
      p = b_prop;
      // TODO(ostava): For now ensure the data type of all components is uint8.
      DCHECK_EQ(true, p->data_type() == DT_UINT8);
      if (p->data_type() != DT_UINT8)
        return false;
      color_readers.push_back(std::unique_ptr<PlyPropertyReader<uint8_t>>(
          new PlyPropertyReader<uint8_t>(p)));
    }
    if (a_prop) {
      p = a_prop;
      // TODO(ostava): For now ensure the data type of all components is uint8.
      DCHECK_EQ(true, p->data_type() == DT_UINT8);
      if (p->data_type() != DT_UINT8)
        return false;
      color_readers.push_back(std::unique_ptr<PlyPropertyReader<uint8_t>>(
          new PlyPropertyReader<uint8_t>(p)));
    }

    GeometryAttribute va;
    va.Init(GeometryAttribute::COLOR, nullptr, num_colors, DT_UINT8, true,
            sizeof(uint8_t) * num_colors, 0);
    const int32_t att_id =
        out_point_cloud_->AddAttribute(va, true, num_vertices);
    for (PointIndex::ValueType i = 0; i < num_vertices; ++i) {
      std::array<uint8_t, 4> val;
      for (int j = 0; j < num_colors; j++) {
        val[j] = color_readers[j]->ReadValue(i);
      }
      out_point_cloud_->attribute(att_id)->SetAttributeValue(
          AttributeValueIndex(i), &val[0]);
    }
  }

  return true;
}

}  // namespace draco
