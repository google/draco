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
#include "compression/decode.h"

#include "compression/config/compression_shared.h"
#include "compression/mesh/mesh_edgebreaker_decoder.h"
#include "compression/mesh/mesh_sequential_decoder.h"
#include "compression/point_cloud/point_cloud_kd_tree_decoder.h"
#include "compression/point_cloud/point_cloud_sequential_decoder.h"

namespace draco {

bool ParseHeader(DecoderBuffer *in_buffer, EncodedGeometryType *out_type,
                 int8_t *out_method) {
  char draco_str[6] = {0};
  if (!in_buffer->Decode(draco_str, 5))
    return false;
  if (strcmp(draco_str, "DRACO") != 0)
    return false;  // Wrong file format?
  uint8_t major_version, minor_version;
  if (!in_buffer->Decode(&major_version))
    return false;
  if (!in_buffer->Decode(&minor_version))
    return false;
  uint8_t encoder_type, encoder_method;
  if (!in_buffer->Decode(&encoder_type))
    return false;
  if (!in_buffer->Decode(&encoder_method))
    return false;
  uint16_t flags;
  if (!in_buffer->Decode(&flags))
    return false;
  *out_type = static_cast<EncodedGeometryType>(encoder_type);
  *out_method = encoder_method;
  return true;
}

EncodedGeometryType GetEncodedGeometryType(DecoderBuffer *in_buffer) {
  DecoderBuffer temp_buffer(*in_buffer);
  EncodedGeometryType geom_type;
  int8_t method;
  if (!ParseHeader(&temp_buffer, &geom_type, &method))
    return INVALID_GEOMETRY_TYPE;
  return geom_type;
}

std::unique_ptr<PointCloudDecoder> CreatePointCloudDecoder(int8_t method) {
  if (method == POINT_CLOUD_SEQUENTIAL_ENCODING) {
    return std::unique_ptr<PointCloudDecoder>(
        new PointCloudSequentialDecoder());
  } else if (method == POINT_CLOUD_KD_TREE_ENCODING) {
    return std::unique_ptr<PointCloudDecoder>(new PointCloudKdTreeDecoder());
  }
  return nullptr;
}

std::unique_ptr<MeshDecoder> CreateMeshDecoder(uint8_t method) {
  if (method == MESH_SEQUENTIAL_ENCODING) {
    return std::unique_ptr<MeshDecoder>(new MeshSequentialDecoder());
  } else if (method == MESH_EDGEBREAKER_ENCODING) {
    return std::unique_ptr<MeshDecoder>(new MeshEdgeBreakerDecoder());
  }
  return nullptr;
}

std::unique_ptr<PointCloud> DecodePointCloudFromBuffer(
    DecoderBuffer *in_buffer) {
  EncodedGeometryType encoder_type;
  int8_t method;
  if (!ParseHeader(in_buffer, &encoder_type, &method))
    return nullptr;
  if (encoder_type == POINT_CLOUD) {
    std::unique_ptr<PointCloudDecoder> decoder =
        CreatePointCloudDecoder(method);
    if (!decoder)
      return nullptr;
    std::unique_ptr<PointCloud> point_cloud(new PointCloud());
    if (!decoder->Decode(in_buffer, point_cloud.get()))
      return nullptr;
    return point_cloud;
  } else if (encoder_type == TRIANGULAR_MESH) {
    std::unique_ptr<MeshDecoder> decoder = CreateMeshDecoder(method);
    if (!decoder)
      return nullptr;
    std::unique_ptr<Mesh> mesh(new Mesh());
    if (!decoder->Decode(in_buffer, mesh.get()))
      return nullptr;
    return std::move(mesh);
  }
  return nullptr;
}

std::unique_ptr<Mesh> DecodeMeshFromBuffer(DecoderBuffer *in_buffer) {
  EncodedGeometryType encoder_type;
  int8_t method;
  if (!ParseHeader(in_buffer, &encoder_type, &method))
    return nullptr;
  std::unique_ptr<MeshDecoder> decoder;
  if (encoder_type == TRIANGULAR_MESH) {
    decoder = CreateMeshDecoder(method);
  }
  if (!decoder)
    return nullptr;
  std::unique_ptr<Mesh> mesh(new Mesh());
  if (!decoder->Decode(in_buffer, mesh.get()))
    return nullptr;
  return mesh;
}

}  // namespace draco
