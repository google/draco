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

#ifdef DRACO_MESH_COMPRESSION_SUPPORTED
#include "compression/mesh/mesh_edgebreaker_decoder.h"
#include "compression/mesh/mesh_sequential_decoder.h"
#endif

#ifdef DRACO_POINT_CLOUD_COMPRESSION_SUPPORTED
#include "compression/point_cloud/point_cloud_kd_tree_decoder.h"
#include "compression/point_cloud/point_cloud_sequential_decoder.h"
#endif

namespace draco {

EncodedGeometryType GetEncodedGeometryType(DecoderBuffer *in_buffer) {
  DecoderBuffer temp_buffer(*in_buffer);
  DracoHeader header;
  if (!PointCloudDecoder::DecodeHeader(&temp_buffer, &header))
    return INVALID_GEOMETRY_TYPE;
  return static_cast<EncodedGeometryType>(header.encoder_type);
}

#ifdef DRACO_POINT_CLOUD_COMPRESSION_SUPPORTED
std::unique_ptr<PointCloudDecoder> CreatePointCloudDecoder(int8_t method) {
  if (method == POINT_CLOUD_SEQUENTIAL_ENCODING) {
    return std::unique_ptr<PointCloudDecoder>(
        new PointCloudSequentialDecoder());
  } else if (method == POINT_CLOUD_KD_TREE_ENCODING) {
    return std::unique_ptr<PointCloudDecoder>(new PointCloudKdTreeDecoder());
  }
  return nullptr;
}
#endif

#ifdef DRACO_MESH_COMPRESSION_SUPPORTED
std::unique_ptr<MeshDecoder> CreateMeshDecoder(uint8_t method) {
  if (method == MESH_SEQUENTIAL_ENCODING) {
    return std::unique_ptr<MeshDecoder>(new MeshSequentialDecoder());
  } else if (method == MESH_EDGEBREAKER_ENCODING) {
    return std::unique_ptr<MeshDecoder>(new MeshEdgeBreakerDecoder());
  }
  return nullptr;
}
#endif

std::unique_ptr<PointCloud> DecodePointCloudFromBuffer(
    DecoderBuffer *in_buffer) {
  DecoderBuffer temp_buffer(*in_buffer);
  DracoHeader header;
  if (!PointCloudDecoder::DecodeHeader(&temp_buffer, &header))
    return nullptr;
  if (header.encoder_type == POINT_CLOUD) {
#ifdef DRACO_POINT_CLOUD_COMPRESSION_SUPPORTED
    std::unique_ptr<PointCloudDecoder> decoder =
        CreatePointCloudDecoder(header.encoder_method);
    if (!decoder)
      return nullptr;
    std::unique_ptr<PointCloud> point_cloud(new PointCloud());
    if (!decoder->Decode(in_buffer, point_cloud.get()))
      return nullptr;
    return point_cloud;
#endif
  } else if (header.encoder_type == TRIANGULAR_MESH) {
#ifdef DRACO_MESH_COMPRESSION_SUPPORTED
    std::unique_ptr<MeshDecoder> decoder =
        CreateMeshDecoder(header.encoder_method);
    if (!decoder)
      return nullptr;
    std::unique_ptr<Mesh> mesh(new Mesh());
    if (!decoder->Decode(in_buffer, mesh.get()))
      return nullptr;
    return std::move(mesh);
#endif
  }
  return nullptr;
}

std::unique_ptr<Mesh> DecodeMeshFromBuffer(DecoderBuffer *in_buffer) {
#ifdef DRACO_MESH_COMPRESSION_SUPPORTED
  DecoderBuffer temp_buffer(*in_buffer);
  DracoHeader header;
  if (!PointCloudDecoder::DecodeHeader(&temp_buffer, &header))
    return nullptr;
  std::unique_ptr<MeshDecoder> decoder;
  if (header.encoder_type == TRIANGULAR_MESH) {
    decoder = CreateMeshDecoder(header.encoder_method);
  }
  if (!decoder)
    return nullptr;
  std::unique_ptr<Mesh> mesh(new Mesh());
  if (!decoder->Decode(in_buffer, mesh.get()))
    return nullptr;
  return mesh;
#else
  return nullptr;
#endif
}

}  // namespace draco
