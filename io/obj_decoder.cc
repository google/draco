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
#include "io/obj_decoder.h"

#include <cctype>
#include <cmath>
#include <fstream>

#include "io/parser_utils.h"

namespace draco {

ObjDecoder::ObjDecoder()
    : counting_mode_(true),
      num_obj_faces_(0),
      num_positions_(0),
      num_tex_coords_(0),
      num_normals_(0),
      pos_att_id_(-1),
      tex_att_id_(-1),
      norm_att_id_(-1),
      material_att_id_(-1),
      deduplicate_input_values_(true),
      last_material_id_(0),
      open_material_file_(false),
      out_mesh_(nullptr),
      out_point_cloud_(nullptr) {}

bool ObjDecoder::DecodeFromFile(const std::string &file_name, Mesh *out_mesh) {
  out_mesh_ = out_mesh;
  return DecodeFromFile(file_name, static_cast<PointCloud *>(out_mesh));
}

bool ObjDecoder::DecodeFromFile(const std::string &file_name,
                                PointCloud *out_point_cloud) {
  std::ifstream file(file_name, std::ios::binary);
  if (!file)
    return false;
  // Read the whole file into a buffer.
  int64_t file_size = file.tellg();
  file.seekg(0, std::ios::end);
  file_size = file.tellg() - file_size;
  if (file_size == 0)
    return false;
  file.seekg(0, std::ios::beg);
  std::vector<char> data(file_size);
  file.read(&data[0], file_size);

  buffer_.Init(&data[0], file_size);

  out_point_cloud_ = out_point_cloud;
  open_material_file_ = true;
  input_file_name_ = file_name;
  return DecodeInternal();
}

bool ObjDecoder::DecodeFromBuffer(DecoderBuffer *buffer, Mesh *out_mesh) {
  out_mesh_ = out_mesh;
  return DecodeFromBuffer(buffer, static_cast<PointCloud *>(out_mesh));
}

bool ObjDecoder::DecodeFromBuffer(DecoderBuffer *buffer,
                                  PointCloud *out_point_cloud) {
  out_point_cloud_ = out_point_cloud;
  buffer_.Init(buffer->data_head(), buffer->remaining_size());
  open_material_file_ = false;
  return DecodeInternal();
}

bool ObjDecoder::DecodeInternal() {
  // In the first pass, count the number of different elements in the geometry.
  // In case the desired output is just a point cloud (i.e., when
  // out_mesh_ == nullptr) the decoder will ignore all information about the
  // connectivity that may be included in the source data.
  counting_mode_ = true;
  ResetCounters();
  material_name_to_id_.clear();
  // Parse all lines.
  bool error = false;
  while (ParseDefinition(&error) && !error) {
  }
  if (error)
    return false;
  if (num_obj_faces_ == 0)
    return true;  // Point cloud is ok.

  // Initialize point cloud and mesh properties.
  if (out_mesh_) {
    // Start decoding a mesh with the given number of faces. For point clouds we
    // silently ignore all data about the mesh connectivity.
    out_mesh_->SetNumFaces(num_obj_faces_);
  }
  out_point_cloud_->set_num_points(3 * num_obj_faces_);
  // Add attributes if they are present in the input data.
  if (num_positions_ > 0) {
    GeometryAttribute va;
    va.Init(GeometryAttribute::POSITION, nullptr, 3, DT_FLOAT32, false,
            sizeof(float) * 3, 0);
    pos_att_id_ = out_point_cloud_->AddAttribute(va, false, num_positions_);
  }
  if (num_tex_coords_ > 0) {
    GeometryAttribute va;
    va.Init(GeometryAttribute::TEX_COORD, nullptr, 2, DT_FLOAT32, false,
            sizeof(float) * 2, 0);
    tex_att_id_ = out_point_cloud_->AddAttribute(va, false, num_tex_coords_);
  }
  if (num_normals_ > 0) {
    GeometryAttribute va;
    va.Init(GeometryAttribute::NORMAL, nullptr, 3, DT_FLOAT32, false,
            sizeof(float) * 3, 0);
    norm_att_id_ = out_point_cloud_->AddAttribute(va, false, num_normals_);
  }
  if (material_name_to_id_.size() > 1) {
    GeometryAttribute va;
    if (material_name_to_id_.size() < 256) {
      va.Init(GeometryAttribute::GENERIC, nullptr, 1, DT_UINT8, false, 1, 0);
    } else if (material_name_to_id_.size() < (1 << 16)) {
      va.Init(GeometryAttribute::GENERIC, nullptr, 1, DT_UINT16, false, 2, 0);
    } else {
      va.Init(GeometryAttribute::GENERIC, nullptr, 1, DT_UINT32, false, 4, 0);
    }
    material_att_id_ =
        out_point_cloud_->AddAttribute(va, false, material_name_to_id_.size());
    // Fill the material entries.
    for (AttributeValueIndex i(0); i < material_name_to_id_.size(); ++i) {
      out_point_cloud_->attribute(material_att_id_)->SetAttributeValue(i, &i);
    }
  }

  // Perform a second iteration of parsing and fill all the data.
  counting_mode_ = false;
  ResetCounters();
  // Start parsing from the beginning of the buffer again.
  buffer()->StartDecodingFrom(0);
  while (ParseDefinition(&error) && !error) {
  }
  if (error)
    return false;
  if (out_mesh_) {
    // Add faces with identity mapping between vertex and corner indices.
    // Duplicate vertices will get removed later.
    Mesh::Face face;
    for (FaceIndex i(0); i < num_obj_faces_; ++i) {
      for (int c = 0; c < 3; ++c)
        face[c] = 3 * i.value() + c;
      out_mesh_->SetFace(i, face);
    }
  }
  if (deduplicate_input_values_) {
    out_point_cloud_->DeduplicateAttributeValues();
  }
  out_point_cloud_->DeduplicatePointIds();
  return true;
}

void ObjDecoder::ResetCounters() {
  num_obj_faces_ = 0;
  num_positions_ = 0;
  num_tex_coords_ = 0;
  num_normals_ = 0;
  last_material_id_ = 0;
}

bool ObjDecoder::ParseDefinition(bool *error) {
  char c;
  parser::SkipWhitespace(buffer());
  if (!buffer()->Peek(&c)) {
    // End of file reached?.
    return false;
  }
  if (c == '#') {
    // Comment, ignore the line.
    parser::SkipLine(buffer());
    return true;
  }
  if (ParseVertexPosition(error))
    return true;
  if (ParseNormal(error))
    return true;
  if (ParseTexCoord(error))
    return true;
  if (ParseFace(error))
    return true;
  if (ParseMaterial(error))
    return true;
  if (ParseMaterialLib(error))
    return true;
  // No known definition was found. Ignore the line.
  parser::SkipLine(buffer());
  return true;
}

bool ObjDecoder::ParseVertexPosition(bool *error) {
  std::array<char, 2> c;
  if (!buffer()->Peek(&c)) {
    return false;
  }
  if (c[0] != 'v' || c[1] != ' ')
    return false;
  // Vertex definition found!
  buffer()->Advance(2);
  if (!counting_mode_) {
    // Parse three float numbers for vertex position coordinates.
    float val[3];
    for (int i = 0; i < 3; ++i) {
      parser::SkipWhitespace(buffer());
      if (!parser::ParseFloat(buffer(), val + i)) {
        *error = true;
        // The definition is processed so return true.
        return true;
      }
    }
    out_point_cloud_->attribute(pos_att_id_)
        ->SetAttributeValue(AttributeValueIndex(num_positions_), val);
  }
  ++num_positions_;
  parser::SkipLine(buffer());
  return true;
}

bool ObjDecoder::ParseNormal(bool *error) {
  std::array<char, 2> c;
  if (!buffer()->Peek(&c)) {
    return false;
  }
  if (c[0] != 'v' || c[1] != 'n')
    return false;
  // Normal definition found!
  buffer()->Advance(2);
  if (!counting_mode_) {
    // Parse three float numbers for the normal vector.
    float val[3];
    for (int i = 0; i < 3; ++i) {
      parser::SkipWhitespace(buffer());
      if (!parser::ParseFloat(buffer(), val + i)) {
        *error = true;
        // The definition is processed so return true.
        return true;
      }
    }
    out_point_cloud_->attribute(norm_att_id_)
        ->SetAttributeValue(AttributeValueIndex(num_normals_), val);
  }
  ++num_normals_;
  parser::SkipLine(buffer());
  return true;
}

bool ObjDecoder::ParseTexCoord(bool *error) {
  std::array<char, 2> c;
  if (!buffer()->Peek(&c)) {
    return false;
  }
  if (c[0] != 'v' || c[1] != 't')
    return false;
  // Texture coord definition found!
  buffer()->Advance(2);
  if (!counting_mode_) {
    // Parse two float numbers for the texture coordinate.
    float val[2];
    for (int i = 0; i < 2; ++i) {
      parser::SkipWhitespace(buffer());
      if (!parser::ParseFloat(buffer(), val + i)) {
        *error = true;
        // The definition is processed so return true.
        return true;
      }
    }
    out_point_cloud_->attribute(tex_att_id_)
        ->SetAttributeValue(AttributeValueIndex(num_tex_coords_), val);
  }
  ++num_tex_coords_;
  parser::SkipLine(buffer());
  return true;
}

bool ObjDecoder::ParseFace(bool *error) {
  char c;
  if (!buffer()->Peek(&c)) {
    return false;
  }
  if (c != 'f')
    return false;
  // Face definition found!
  buffer()->Advance(1);
  if (!counting_mode_) {
    // Parse face indices.
    for (int i = 0; i < 3; ++i) {
      const PointIndex vert_id(3 * num_obj_faces_ + i);
      parser::SkipWhitespace(buffer());
      std::array<int32_t, 3> indices;
      if (!ParseVertexIndices(&indices)) {
        *error = true;
        return true;
      }
      // Use face entries to store mapping between vertex and attribute indices.
      if (indices[0] > 0) {
        out_point_cloud_->attribute(pos_att_id_)
            ->SetPointMapEntry(vert_id, AttributeValueIndex(indices[0] - 1));
      }
      if (indices[1] > 0) {
        out_point_cloud_->attribute(tex_att_id_)
            ->SetPointMapEntry(vert_id, AttributeValueIndex(indices[1] - 1));
      }
      if (indices[2] > 0) {
        out_point_cloud_->attribute(norm_att_id_)
            ->SetPointMapEntry(vert_id, AttributeValueIndex(indices[2] - 1));
      }
      if (material_att_id_ >= 0) {
        out_point_cloud_->attribute(material_att_id_)
            ->SetPointMapEntry(vert_id, AttributeValueIndex(last_material_id_));
      }
    }
  }
  ++num_obj_faces_;
  parser::SkipLine(buffer());
  return true;
}

bool ObjDecoder::ParseMaterialLib(bool *error) {
  // Allow only one material library per file for now.
  if (material_name_to_id_.size() > 0)
    return false;
  // Skip the parsing if we don't want to open material files.
  if (!open_material_file_)
    return false;
  std::array<char, 6> c;
  if (!buffer()->Peek(&c)) {
    return false;
  }
  if (std::memcmp(&c[0], "mtllib", 6) != 0)
    return false;
  buffer()->Advance(6);
  parser::SkipWhitespace(buffer());
  std::string mat_file_name;
  if (!parser::ParseString(buffer(), &mat_file_name)) {
    *error = true;
    return true;
  }
  parser::SkipLine(buffer());

  if (mat_file_name.size() > 0) {
    if (!ParseMaterialFile(mat_file_name, error)) {
      // Silently ignore problems with material files for now.
      return true;
    }
  }
  return true;
}

bool ObjDecoder::ParseMaterial(bool *error) {
  if (counting_mode_)
    return false;  // Skip when we are counting definitions.
  if (material_att_id_ < 0)
    return false;  // Don't parse it when we don't use materials.
  std::array<char, 6> c;
  if (!buffer()->Peek(&c)) {
    return false;
  }
  if (std::memcmp(&c[0], "usemtl", 6) != 0)
    return false;
  buffer()->Advance(6);
  parser::SkipWhitespace(buffer());
  std::string mat_name;
  if (!parser::ParseString(buffer(), &mat_name))
    return false;
  auto it = material_name_to_id_.find(mat_name);
  if (it == material_name_to_id_.end()) {
    // Invalid material..ignore.
    return true;
  }
  last_material_id_ = it->second;
  return true;
}

bool ObjDecoder::ParseVertexIndices(std::array<int32_t, 3> *out_indices) {
  // Parsed attribute indices can be in format:
  // 1. POS_INDEX
  // 2. POS_INDEX/TEX_COORD_INDEX
  // 3. POS_INDEX/TEX_COORD_INDEX/NORMAL_INDEX
  // 4. POS_INDEX//NORMAL_INDEX
  parser::SkipWhitespace(buffer());
  if (!parser::ParseSignedInt(buffer(), &(*out_indices)[0]) ||
      (*out_indices)[0] < 1)
    return false;  // Position index must be present and valid.
  (*out_indices)[1] = (*out_indices)[2] = 0;
  char ch;
  if (!buffer()->Peek(&ch))
    return true;  // It may be ok if we cannot read any more characters.
  if (ch != '/')
    return true;
  buffer()->Advance(1);
  // Check if we should skip texture index or not.
  if (!buffer()->Peek(&ch))
    return false;  // Here, we should be always able to read the next char.
  if (ch != '/') {
    // Must be texture coord index.
    if (!parser::ParseSignedInt(buffer(), &(*out_indices)[1]) ||
        (*out_indices)[1] < 1)
      return false;  // Texture index must be present and valid.
  }
  if (!buffer()->Peek(&ch))
    return true;
  if (ch == '/') {
    buffer()->Advance(1);
    // Read normal index.
    if (!parser::ParseSignedInt(buffer(), &(*out_indices)[2]) ||
        (*out_indices)[2] < 1)
      return false;  // Normal index must be present and valid.
  }
  return true;
}

bool ObjDecoder::ParseMaterialFile(const std::string &file_name, bool *error) {
  // Get the correct path to the |file_name| using the folder from
  // |input_file_name_| as the root folder.
  const auto pos = input_file_name_.find_last_of("/\\");
  std::string full_path;
  if (pos != std::string::npos) {
    full_path = input_file_name_.substr(0, pos + 1);
  }
  full_path += file_name;

  std::ifstream file(full_path, std::ios::binary);
  if (!file)
    return false;
  // Read the whole file into a buffer.
  file.seekg(0, std::ios::end);
  const std::string::size_type file_size = file.tellg();
  if (file_size == 0)
    return false;
  file.seekg(0, std::ios::beg);
  std::vector<char> data(file_size);
  file.read(&data[0], file_size);

  // Backup the original decoder buffer.
  DecoderBuffer old_buffer = buffer_;

  buffer_.Init(&data[0], file_size);

  while (ParseMaterialFileDefinition(error))
    ;

  // Restore the original buffer.
  buffer_ = old_buffer;
  return true;
}

bool ObjDecoder::ParseMaterialFileDefinition(bool *error) {
  char c;
  parser::SkipWhitespace(buffer());
  if (!buffer()->Peek(&c)) {
    // End of file reached?.
    return false;
  }
  if (c == '#') {
    // Comment, ignore the line.
    parser::SkipLine(buffer());
    return true;
  }
  std::string str;
  if (!parser::ParseString(buffer(), &str))
    return false;
  if (str.compare("newmtl") == 0) {
    parser::SkipWhitespace(buffer());
    if (!parser::ParseString(buffer(), &str))
      return false;
    // Add new material to our map.
    material_name_to_id_[str] = material_name_to_id_.size();
  }
  parser::SkipLine(buffer());
  return true;
}

}  // namespace draco
