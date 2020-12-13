// Copyright 2020 The Draco Authors.
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

#include "draco/draco_features.h"

#ifdef DRACO_C_API_SUPPORTED

#include <cstring>

#include "draco/c_api/c_api.h"
#include "draco/compression/decode.h"

void dracoStatusRelease(draco_status *status) {
  free(status);
}

int dracoStatusCode(const draco_status *status) {
  return reinterpret_cast<const draco::Status*>(status)->code();
}

bool dracoStatusOk(const draco_status *status) {
  return reinterpret_cast<const draco::Status*>(status)->ok();
}

draco_string dracoStatusErrorMsg(const draco_status *status) {
  auto msg = reinterpret_cast<const draco::Status*>(status)->error_msg();
  return reinterpret_cast<draco_string>(msg);
}

draco_mesh* dracoNewMesh() {
  return reinterpret_cast<draco_mesh*>(new draco::Mesh());
}

void dracoMeshRelease(draco_mesh *mesh) {
  free(mesh);
}

uint32_t dracoMeshNumFaces(const draco_mesh *mesh) {
  return reinterpret_cast<const draco::Mesh*>(mesh)->num_faces();
}


uint32_t dracoMeshNumPoints(const draco_mesh *mesh) {
  return reinterpret_cast<const draco::Mesh*>(mesh)->num_points();
}

template <typename T>
bool GetTrianglesArray(const draco::Mesh *m, const size_t out_size,
                       T *out_values) {
  const uint32_t num_faces = m->num_faces();
  if (num_faces * 3 * sizeof(T) != out_size) {
    return false;
  }

  for (uint32_t face_id = 0; face_id < num_faces; ++face_id) {
    const draco::Mesh::Face &face = m->face(draco::FaceIndex(face_id));
    out_values[face_id * 3 + 0] = static_cast<T>(face[0].value());
    out_values[face_id * 3 + 1] = static_cast<T>(face[1].value());
    out_values[face_id * 3 + 2] = static_cast<T>(face[2].value());
  }
  return true;
}

bool dracoMeshGetTrianglesUint16(const draco_mesh *mesh, const size_t out_size,
                                 uint16_t *out_values) {
  auto m = reinterpret_cast<const draco::Mesh*>(mesh);
  if (m->num_points() > std::numeric_limits<uint16_t>::max()) {
    return false;
  }

  return GetTrianglesArray(m, out_size, out_values);
}

bool dracoMeshGetTrianglesUint32(const draco_mesh *mesh, const size_t out_size,
                                 uint32_t *out_values) {
  auto m = reinterpret_cast<const draco::Mesh*>(mesh);
  if (m->num_points() > std::numeric_limits<uint32_t>::max()) {
    return false;
  }

  return GetTrianglesArray(m, out_size, out_values);
}

draco_decoder* dracoNewDecoder() {
  return reinterpret_cast<draco_decoder*>(new draco::Decoder());
}

void dracoDecoderRelease(draco_decoder *decoder) {
  free(decoder);
}

draco_status* dracoDecoderArrayToMesh(draco_decoder *decoder, 
                                  const char *data, 
                                  size_t data_size,
                                  draco_mesh *out_mesh) {
  draco::DecoderBuffer buffer;
  buffer.Init(data, data_size);
  auto m = reinterpret_cast<draco::Mesh*>(out_mesh);
  const auto &last_status_ = reinterpret_cast<draco::Decoder*>(decoder)->DecodeBufferToGeometry(&buffer, m);
  return reinterpret_cast<draco_status*>(new draco::Status(last_status_));
}

#endif // DRACO_C_API_SUPPORTED