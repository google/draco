// Copyright 2022 The Draco Authors.
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
#include <iomanip>
#include <sstream>

#include "draco/io/file_writer_factory.h"
#include "draco/io/file_writer_interface.h"

namespace draco {

StlEncoder::StlEncoder()
    : out_buffer_(nullptr), in_point_cloud_(nullptr), in_mesh_(nullptr) {}

Status StlEncoder::EncodeToFile(const Mesh &mesh, const std::string &file_name) {
  in_mesh_ = &mesh;
  std::unique_ptr<FileWriterInterface> file =
      FileWriterFactory::OpenWriter(file_name);
  if (!file) {
    return Status(Status::IO_ERROR, "File couldn't be opened");
  }
  // Encode the mesh into a buffer.
  EncoderBuffer buffer;
  DRACO_RETURN_IF_ERROR(EncodeToBuffer(mesh, &buffer));
  // Write the buffer into the file.
  file->Write(buffer.data(), buffer.size());
  return OkStatus();
}

Status StlEncoder::EncodeToBuffer(const Mesh &mesh, EncoderBuffer *out_buffer) {
  in_mesh_ = &mesh;
  out_buffer_ = out_buffer;
  Status s = EncodeInternal();
  in_mesh_ = nullptr; // cleanup
  in_point_cloud_ = nullptr;
  out_buffer_ = nullptr;
  return s;
}

Status StlEncoder::EncodeInternal() {
  // Write STL header.
  std::stringstream out;
  out << std::left << std::setw(80) << "generated using Draco";  // header is 80 bytes fixed size.
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
    return ErrorStatus("Mesh is missing the position attribute.");
  }

  // Ensure normals are 3 component. Don't encode them otherwise.
  if (normal_att_id >= 0 &&
      in_mesh_->attribute(normal_att_id)->num_components() != 3) {
    normal_att_id = -1;
  }

  if (in_mesh_->attribute(pos_att_id)->data_type() !=
      DT_FLOAT32) {
    return ErrorStatus("Mesh position attribute is not of type float32.");
  }

  if (normal_att_id >= 0 &&
      in_mesh_->attribute(normal_att_id)->data_type() != DT_FLOAT32) {
    return ErrorStatus("Mesh normal attribute is not of type float32.");
  }

  uint16_t unused = 0;

  if (in_mesh_) {
    for (FaceIndex i(0); i < in_mesh_->num_faces(); ++i) {

      const auto &f = in_mesh_->face(i);
      const auto *const pos_att = in_mesh_->attribute(pos_att_id);

      if (normal_att_id >= 0) {
        const auto *const normal_att =
            in_mesh_->attribute(normal_att_id);
        buffer()->Encode(normal_att->GetAddress(normal_att->mapped_index(f[0])),
                         normal_att->byte_stride());
      } else {
        // In case the source data does not contain normal vectors, we calculate them
        // using the points of the trianlge face: cross(p2-p1, p3-p1)
        // Finally we normalize the vector.
        // TODO: can Eigen::Vector3f math be used here ?

        std::array<float, 3> p1 =
            pos_att->GetValue<float, 3>(pos_att->mapped_index(f[0]));
        std::array<float, 3> p2 = pos_att->GetValue<float, 3>(pos_att->mapped_index(f[1]));
        std::array<float, 3> p3 = pos_att->GetValue<float, 3>(pos_att->mapped_index(f[2]));
        std::array<float, 3> a = {p2[0] - p1[0], p2[1] - p1[1], p2[2] - p1[2]};
        std::array<float, 3> b = {p3[0] - p1[0], p3[1] - p1[1], p3[2] - p1[2]};
        std::array<float, 3> c = {a[1] * b[2] - a[2] * b[1],
                                      a[2] * b[0] - a[0] * b[2],
                                      a[0] * b[1] - a[1] * b[0]};
        float len = sqrt(c[0] * c[0] + c[1] * c[1] + c[2] * c[2]);
        if (len > 0) {
          c = {c[0] / len, c[1] / len, c[2] / len};
        } else {
          // Degenerate data in position attribute, let's use a default normalized vector. 
          c = {0, 0, 1};
        }
        buffer()->Encode(c);
      }

      for (int c = 0; c < 3; ++c) {
        buffer()->Encode(pos_att->GetAddress(pos_att->mapped_index(f[c])),
                         pos_att->byte_stride());
      }

      buffer()->Encode(&unused, 2);

    }
  }
  return OkStatus();
}

}  // namespace draco
