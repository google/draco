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
#include "draco/io/stl_encoder.h"

#include <memory>
#include <sstream>

#include "draco/io/file_writer_factory.h"
#include "draco/io/file_writer_interface.h"

namespace draco {

StlEncoder::StlEncoder()
    : out_buffer_(nullptr), in_point_cloud_(nullptr), in_mesh_(nullptr) {}

bool StlEncoder::EncodeToFile(const Mesh &mesh, const std::string &file_name) {
  in_mesh_ = &mesh;
  std::unique_ptr<FileWriterInterface> file =
      FileWriterFactory::OpenWriter(file_name);
  if (!file) {
    return false;  // File couldn't be opened.
  }
  // Encode the mesh into a buffer.
  EncoderBuffer buffer;
  if (!EncodeToBuffer(mesh, &buffer)) {
    return false;
  }
  // Write the buffer into the file.
  file->Write(buffer.data(), buffer.size());
  return true;
}

bool StlEncoder::EncodeToBuffer(const Mesh &mesh, EncoderBuffer *out_buffer) {
  in_mesh_ = &mesh;
  out_buffer_ = out_buffer;
  if (!EncodeInternal()) {
    return ExitAndCleanup(false);
  }
  return ExitAndCleanup(true);
}
bool StlEncoder::EncodeInternal() {
  // Write STL header.
  std::stringstream out;
  out << "generated using draco_decoder           "; // 40 bytes +
  out << "                                        "; // 40 bytes == header
  const std::string header_str = out.str();
  buffer()->Encode(header_str.data(), header_str.length());

  uint32_t num_faces = in_mesh_->num_faces();
  buffer()->Encode(&num_faces, 4);

  std::vector<uint8_t> stl_face;

  const int pos_att_id =
      in_mesh_->GetNamedAttributeId(GeometryAttribute::POSITION);
  int normal_att_id =
      in_mesh_->GetNamedAttributeId(GeometryAttribute::NORMAL);

  if (pos_att_id < 0) {
    return false;
  }

  // Ensure normals are 3 component. Don't encode them otherwise.
  if (normal_att_id >= 0 &&
      in_mesh_->attribute(normal_att_id)->num_components() != 3) {
    normal_att_id = -1;
  }

  if (in_mesh_->attribute(pos_att_id)->data_type() !=
      DT_FLOAT32) {
    return false;
  }

  if (in_mesh_->attribute(normal_att_id)->data_type() != DT_FLOAT32) {
    return false;
  }

  uint16_t unused = 0;

  if (in_mesh_) {
    for (FaceIndex i(0); i < in_mesh_->num_faces(); ++i) {

      const auto &f = in_mesh_->face(i);

      if (normal_att_id >= 0) {
        const auto *const normal_att =
            in_mesh_->attribute(normal_att_id);
        buffer()->Encode(normal_att->GetAddress(normal_att->mapped_index(f[0])),
                         normal_att->byte_stride());
      }

      const auto *const pos_att = in_mesh_->attribute(pos_att_id);
      for (int c = 0; c < 3; ++c) {
        buffer()->Encode(pos_att->GetAddress(pos_att->mapped_index(f[c])),
                         pos_att->byte_stride());
      }

      buffer()->Encode(&unused, 2);

    }
  }
  return true;
}

bool StlEncoder::ExitAndCleanup(bool return_value) {
  in_mesh_ = nullptr;
  in_point_cloud_ = nullptr;
  out_buffer_ = nullptr;
  return return_value;
}


}  // namespace draco
