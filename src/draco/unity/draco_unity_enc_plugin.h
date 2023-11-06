// Copyright 2021 The Draco Authors.
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
#ifndef DRACO_UNITY_ENC_DRACO_UNITY_PLUGIN_H_
#define DRACO_UNITY_ENC_DRACO_UNITY_PLUGIN_H_

// No idea why, but if this (or any other draco header) is not included DRACO_UNITY_PLUGIN is not defined
#include "draco/core/draco_types.h"
#include "draco/mesh/mesh.h"
#include "draco/core/encoder_buffer.h"
#include "draco/compression/encode.h"


#ifdef DRACO_UNITY_PLUGIN

// If compiling with Visual Studio.
#if defined(_MSC_VER)
#define EXPORT_API __declspec(dllexport)
#else
// Other platforms don't need this.
#define EXPORT_API
#endif  // defined(_MSC_VER)

namespace draco {

  struct DracoEncoder
  {
      uint32_t encodedVertices;
      uint32_t encodedIndices;
      std::vector<std::unique_ptr<draco::DataBuffer>> buffers;
      draco::EncoderBuffer encoderBuffer;
      uint32_t encodingSpeed = 0;
      uint32_t decodingSpeed = 0;
      std::size_t rawSize = 0;
      struct
      {
          uint32_t position = 14;
          uint32_t normal = 10;
          uint32_t uv = 12;
          uint32_t color = 10;
          uint32_t generic = 12;
      } quantization;
      bool is_point_cloud;
  };

  struct DracoMeshEncoder : DracoEncoder {
    draco::Mesh mesh;
  };

  struct DracoPointsEncoder : DracoEncoder {
    draco::PointCloud mesh;
  };

  template<class T>
  void dracoEncodeIndices(DracoMeshEncoder *encoder, uint32_t indexCount,
                          bool flip, T *indices);
  template <class T>
  uint32_t dracoSetAttribute(T *encoder, GeometryAttribute::Type attributeType,
                                    draco::DataType dracoDataType,
                                    int32_t componentCount, int32_t stride,
                                    bool flip,
                                    void *data);

  template<class T>
  bool dracoEncode(T *encoder, uint8_t sequentialEncode);


extern "C" {

  EXPORT_API DracoEncoder *dracoEncoderCreate(uint32_t vertexCount);
  EXPORT_API DracoEncoder *dracoEncoderCreatePointCloud(uint32_t pointCount);
  void EXPORT_API dracoEncoderRelease(DracoEncoder *encoder);
  void EXPORT_API dracoEncoderSetCompressionSpeed(DracoEncoder *encoder, uint32_t encodingSpeed, uint32_t decodingSpeed);
  void EXPORT_API dracoEncoderSetQuantizationBits(DracoEncoder *encoder, uint32_t position, uint32_t normal, uint32_t uv, uint32_t color, uint32_t generic);
  bool EXPORT_API dracoEncoderEncode(DracoEncoder *encoder, uint8_t preserveTriangleOrder);
  uint32_t EXPORT_API dracoEncoderGetEncodedVertexCount(DracoEncoder *encoder);
  uint32_t EXPORT_API dracoEncoderGetEncodedIndexCount(DracoEncoder *encoder);
  uint64_t EXPORT_API dracoEncoderGetByteLength(DracoEncoder *encoder);
  void EXPORT_API dracoEncoderCopy(DracoEncoder *encoder, uint8_t *data);
  void EXPORT_API dracoEncoderGetEncodeBuffer(DracoEncoder *encoder, void **data, uint64_t *size);
  bool EXPORT_API dracoEncoderSetIndices(DracoMeshEncoder *encoder,
                                         DataType indexComponentType,
                                         uint32_t indexCount,
                                         bool flip,
                                         void *indices);
  uint32_t EXPORT_API dracoEncoderSetAttribute(
    DracoEncoder *encoder,
    GeometryAttribute::Type attributeType,
    draco::DataType dracoDataType,
    int32_t componentCount,
    int32_t stride,
    bool flip,
    void *data
    );
  }  // extern "C"

}  // namespace draco

#endif  // DRACO_UNITY_PLUGIN

#endif  // DRACO_UNITY_ENC_DRACO_UNITY_PLUGIN_H_
