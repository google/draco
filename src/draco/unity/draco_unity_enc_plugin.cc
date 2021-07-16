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

#include "draco/unity/draco_unity_enc_plugin.h"

#ifdef DRACO_UNITY_PLUGIN

#include <memory>

namespace draco {

  DracoEncoder * dracoEncoderCreate(uint32_t vertexCount)
  {
      DracoEncoder *encoder = new DracoEncoder;
      encoder->mesh.set_num_points(vertexCount);
      return encoder;
  }

  void dracoEncoderRelease(DracoEncoder *encoder)
  {
      delete encoder;
  }

  void dracoEncoderSetCompressionSpeed(DracoEncoder *encoder, uint32_t encodingSpeed, uint32_t decodingSpeed) {
      encoder->encodingSpeed = encodingSpeed;
      encoder->decodingSpeed = decodingSpeed;
  }

  void dracoEncoderSetQuantizationBits(DracoEncoder *encoder, uint32_t position, uint32_t normal, uint32_t uv, uint32_t color, uint32_t generic)
  {
      encoder->quantization.position = position;
      encoder->quantization.normal = normal;
      encoder->quantization.uv = uv;
      encoder->quantization.color = color;
      encoder->quantization.generic = generic;
  }

  bool dracoEncoderEncode(DracoEncoder *encoder, uint8_t preserveTriangleOrder)
  {
      draco::Encoder dracoEncoder;
      dracoEncoder.SetSpeedOptions(encoder->encodingSpeed, encoder->decodingSpeed);
      dracoEncoder.SetAttributeQuantization(draco::GeometryAttribute::POSITION, encoder->quantization.position);
      dracoEncoder.SetAttributeQuantization(draco::GeometryAttribute::NORMAL, encoder->quantization.normal);
      dracoEncoder.SetAttributeQuantization(draco::GeometryAttribute::TEX_COORD, encoder->quantization.uv);
      dracoEncoder.SetAttributeQuantization(draco::GeometryAttribute::COLOR, encoder->quantization.color);
      dracoEncoder.SetAttributeQuantization(draco::GeometryAttribute::GENERIC, encoder->quantization.generic);
      dracoEncoder.SetTrackEncodedProperties(true);
      
      if (preserveTriangleOrder) {
          dracoEncoder.SetEncodingMethod(draco::MESH_SEQUENTIAL_ENCODING);
      }
      
      auto encoderStatus = dracoEncoder.EncodeMeshToBuffer(encoder->mesh, &encoder->encoderBuffer);
      if (encoderStatus.ok()) {
          encoder->encodedVertices = static_cast<uint32_t>(dracoEncoder.num_encoded_points());
          encoder->encodedIndices = static_cast<uint32_t>(dracoEncoder.num_encoded_faces() * 3);
          return true;
      } else {
          return false;
      }
  }

  uint32_t dracoEncoderGetEncodedVertexCount(DracoEncoder *encoder)
  {
      return encoder->encodedVertices;
  }

  uint32_t dracoEncoderGetEncodedIndexCount(DracoEncoder *encoder)
  {
      return encoder->encodedIndices;
  }

  uint64_t dracoEncoderGetByteLength(DracoEncoder *encoder)
  {
      return encoder->encoderBuffer.size();
  }

  void dracoEncoderCopy(DracoEncoder *encoder, uint8_t *data)
  {
      memcpy(data, encoder->encoderBuffer.data(), encoder->encoderBuffer.size());
  }

  bool dracoEncodeIndices(DracoEncoder *encoder, uint32_t indexCount, DataType indexType, void *indices)
  {
    switch (indexType)
    {
    case DataType::DT_UINT16:
      dracoEncodeIndices<uint16_t>(encoder,indexCount,static_cast<uint16_t*>(indices));
      break;
    case DataType::DT_UINT32:
      dracoEncodeIndices<uint32_t>(encoder,indexCount,static_cast<uint32_t*>(indices));
      break;
    default:
      return false;
    }

    return true;
  }

  template<class T>
  void dracoEncodeIndices(DracoEncoder *encoder, uint32_t indexCount, T *indices)
  {
      int face_count = indexCount / 3;
      encoder->mesh.SetNumFaces(static_cast<size_t>(face_count));
      encoder->rawSize += indexCount * sizeof(T);
      
      for (int i = 0; i < face_count; ++i)
      {
          draco::Mesh::Face face = {
              draco::PointIndex(indices[3 * i + 0]),
              draco::PointIndex(indices[3 * i + 1]),
              draco::PointIndex(indices[3 * i + 2])
          };
          encoder->mesh.SetFace(draco::FaceIndex(static_cast<uint32_t>(i)), face);
      }
  }

  bool dracoEncoderSetIndices(DracoEncoder *encoder, DataType indexComponentType, uint32_t indexCount, void *indices)
  {
      switch (indexComponentType)
      {
          case DataType::DT_INT8:
              dracoEncodeIndices(encoder, indexCount, reinterpret_cast<int8_t *>(indices));
              break;
          case DataType::DT_UINT8:
              dracoEncodeIndices(encoder, indexCount, reinterpret_cast<uint8_t *>(indices));
              break;
          case DataType::DT_INT16:
              dracoEncodeIndices(encoder, indexCount, reinterpret_cast<int16_t *>(indices));
              break;
          case DataType::DT_UINT16:
              dracoEncodeIndices(encoder, indexCount, reinterpret_cast<uint16_t *>(indices));
              break;
          case DataType::DT_UINT32:
              dracoEncodeIndices(encoder, indexCount, reinterpret_cast<uint32_t *>(indices));
              break;
          default:
              return false;
      }
      return true;
  }

  uint32_t dracoEncoderSetAttribute(DracoEncoder *encoder, GeometryAttribute::Type attributeType, draco::DataType dracoDataType, int32_t componentCount, int32_t stride, void *data)
  {
      auto buffer = std::unique_ptr<draco::DataBuffer>( new draco::DataBuffer());
      uint32_t count = encoder->mesh.num_points();

      draco::GeometryAttribute attribute;
      attribute.Init(attributeType, &*buffer, componentCount, dracoDataType, false, stride, 0);

      auto id = static_cast<uint32_t>(encoder->mesh.AddAttribute(attribute, true, count));
      auto dataBytes = reinterpret_cast<uint8_t *>(data);

      for (uint32_t i = 0; i < count; i++)
      {
          encoder->mesh.attribute(id)->SetAttributeValue(draco::AttributeValueIndex(i), dataBytes + i * stride);
      }

      encoder->buffers.emplace_back(std::move(buffer));
      encoder->rawSize += count * stride;
      return id;
  }

}  // namespace draco

#endif  // DRACO_UNITY_PLUGIN
