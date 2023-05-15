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

  DracoEncoder *dracoEncoderCreate(uint32_t vertexCount)
  {
    DracoMeshEncoder *encoder = new DracoMeshEncoder;
    encoder->is_point_cloud = false;
    encoder->mesh.set_num_points(vertexCount);
    return encoder;
  }

  DracoEncoder *dracoEncoderCreatePointCloud(uint32_t vertexCount) {
    DracoPointsEncoder *encoder = new DracoPointsEncoder;
    encoder->is_point_cloud = true;
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

  bool dracoEncoderEncode(DracoEncoder *encoder, uint8_t sequentialEncode)
  {
    if (encoder->is_point_cloud) {
      return dracoEncode((DracoPointsEncoder *)encoder, sequentialEncode);
    } else {
      return dracoEncode((DracoMeshEncoder *)encoder, sequentialEncode);
    }
  }

  template<class T>
  bool dracoEncode(T *encoderT, uint8_t sequentialEncode) {
    auto encoder = (DracoEncoder *)encoderT;
    draco::Encoder dracoEncoder;
    dracoEncoder.SetSpeedOptions(encoder->encodingSpeed,
                                 encoder->decodingSpeed);
    dracoEncoder.SetAttributeQuantization(draco::GeometryAttribute::POSITION,
                                          encoder->quantization.position);
    dracoEncoder.SetAttributeQuantization(draco::GeometryAttribute::NORMAL,
                                          encoder->quantization.normal);
    dracoEncoder.SetAttributeQuantization(draco::GeometryAttribute::TEX_COORD,
                                          encoder->quantization.uv);
    dracoEncoder.SetAttributeQuantization(draco::GeometryAttribute::COLOR,
                                          encoder->quantization.color);
    dracoEncoder.SetAttributeQuantization(draco::GeometryAttribute::GENERIC,
                                          encoder->quantization.generic);
    dracoEncoder.SetTrackEncodedProperties(true);

    draco::Status encodeStatus;

    if (encoder->is_point_cloud) {
      if (sequentialEncode) {
        dracoEncoder.SetEncodingMethod(draco::POINT_CLOUD_SEQUENTIAL_ENCODING);
      } else {
        dracoEncoder.SetEncodingMethod(draco::POINT_CLOUD_KD_TREE_ENCODING);
      }

      encodeStatus = dracoEncoder.EncodePointCloudToBuffer(
          ((DracoPointsEncoder *)encoder)->mesh, &encoder->encoderBuffer);
    } else {
      if (sequentialEncode) {
        dracoEncoder.SetEncodingMethod(draco::MESH_SEQUENTIAL_ENCODING);
      } else {
        dracoEncoder.SetEncodingMethod(draco::MESH_EDGEBREAKER_ENCODING);
      }

      encodeStatus = dracoEncoder.EncodeMeshToBuffer(
          ((DracoMeshEncoder *)encoder)->mesh, &encoder->encoderBuffer);
    }

    if (encodeStatus.ok()) {
      encoder->encodedVertices =
          static_cast<uint32_t>(dracoEncoder.num_encoded_points());
      encoder->encodedIndices =
          static_cast<uint32_t>(dracoEncoder.num_encoded_faces() * 3);
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

  void dracoEncoderGetEncodeBuffer(DracoEncoder *encoder, void **data, uint64_t *size)
  {
      *data = (void*) encoder->encoderBuffer.data();
      *size = encoder->encoderBuffer.size();
  }

  bool dracoEncodeIndices(DracoMeshEncoder *encoder, uint32_t indexCount, DataType indexType, bool flip, void *indices)
  {
    switch (indexType)
    {
    case DataType::DT_UINT16:
      dracoEncodeIndices<uint16_t>(encoder,indexCount,flip,static_cast<uint16_t*>(indices));
      break;
    case DataType::DT_UINT32:
      dracoEncodeIndices<uint32_t>(encoder,indexCount,flip,static_cast<uint32_t*>(indices));
      break;
    default:
      return false;
    }

    return true;
  }

  template<class T>
  void dracoEncodeIndices(DracoMeshEncoder *encoder, uint32_t indexCount, bool flip, T *indices)
  {
      int face_count = indexCount / 3;
      encoder->mesh.SetNumFaces(static_cast<size_t>(face_count));
      encoder->rawSize += indexCount * sizeof(T);
      
      if(flip) {
        for (int i = 0; i < face_count; ++i)
        {
            draco::Mesh::Face face = {
                draco::PointIndex(indices[3 * i + 0]),
                draco::PointIndex(indices[3 * i + 2]),
                draco::PointIndex(indices[3 * i + 1])
            };
            encoder->mesh.SetFace(draco::FaceIndex(static_cast<uint32_t>(i)), face);
        }
      }
      else {
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
  }

  bool dracoEncoderSetIndices(DracoMeshEncoder *encoder, DataType indexComponentType, uint32_t indexCount, bool flip, void *indices)
  {
      switch (indexComponentType)
      {
          case DataType::DT_INT8:
              dracoEncodeIndices(encoder, indexCount, flip, reinterpret_cast<int8_t *>(indices));
              break;
          case DataType::DT_UINT8:
              dracoEncodeIndices(encoder, indexCount, flip, reinterpret_cast<uint8_t *>(indices));
              break;
          case DataType::DT_INT16:
              dracoEncodeIndices(encoder, indexCount, flip, reinterpret_cast<int16_t *>(indices));
              break;
          case DataType::DT_UINT16:
              dracoEncodeIndices(encoder, indexCount, flip, reinterpret_cast<uint16_t *>(indices));
              break;
          case DataType::DT_UINT32:
              dracoEncodeIndices(encoder, indexCount, flip, reinterpret_cast<uint32_t *>(indices));
              break;
          default:
              return false;
      }
      return true;
  }

  
  uint32_t dracoEncoderSetAttribute(DracoEncoder *encoder,
                                    GeometryAttribute::Type attributeType,
                                    draco::DataType dracoDataType,
                                    int32_t componentCount, int32_t stride,
                                    bool flip,
                                    void *data) {
    if (encoder->is_point_cloud) {
      return dracoSetAttribute<DracoPointsEncoder>(
          (DracoPointsEncoder*)encoder, attributeType, dracoDataType,
                               componentCount, stride, flip, data);
    } else {
      return dracoSetAttribute((DracoMeshEncoder *)encoder, attributeType,
                               dracoDataType,
                               componentCount, stride, flip, data);
    }

  }

  template <class E, typename T>
  void SetAttributeValuesFlipped(E *encoder, uint32_t id, uint32_t count, int32_t componentCount, int32_t stride, uint8_t * dataBytes) {

    switch (componentCount) {
      case 2: 
      { 
        T tmp[2];
        for (uint32_t i = 0; i < count; i++)
        {
          T* srcPtr = (T*) (dataBytes + i * stride);
          // Texture coordinate left-handed lower left to right-handed top left conversion
          tmp[0] = *srcPtr;
          tmp[1] = ((T)1) - (*(srcPtr+1));
          encoder->mesh.attribute(id)->SetAttributeValue(draco::AttributeValueIndex(i), tmp);
        }
        break;
      }
      case 3:
      {
        T tmp[3];
        for (uint32_t i = 0; i < count; i++)
        {
          T* srcPtr = (T*) (dataBytes + i * stride);
          // Position/Normal left-hand to right-handed coordinate system switch: flip X axis
          tmp[0] = -(*srcPtr);
          tmp[1] = *(srcPtr+1);
          tmp[2] = *(srcPtr+2);
          encoder->mesh.attribute(id)->SetAttributeValue(draco::AttributeValueIndex(i), tmp);
        }
        break;
      }
      case 4:
      {
        T tmp[4];
        for (uint32_t i = 0; i < count; i++)
        {
          T* srcPtr = (T*) (dataBytes + i * stride);
          // Tangent left-hand to right-handed coordinate system switch: flip Y and Z axis
          tmp[0] = *(srcPtr+0);
          tmp[1] = -*(srcPtr+1);
          tmp[2] = -*(srcPtr+2);
          tmp[3] = *(srcPtr+3);
          encoder->mesh.attribute(id)->SetAttributeValue(draco::AttributeValueIndex(i), tmp);
        }
        break;
      }
      default:
      {
        for (uint32_t i = 0; i < count; i++)
        {
          encoder->mesh.attribute(id)->SetAttributeValue(draco::AttributeValueIndex(i), dataBytes + i * stride);
        }
        break;
      }
    }
  }

  template <class T>
  uint32_t dracoSetAttribute(T *encoder,
                                    GeometryAttribute::Type attributeType,
                                    draco::DataType dracoDataType,
                                    int32_t componentCount, int32_t stride,
                                    bool flip,
                                    void *data)
  {
      auto buffer = std::unique_ptr<draco::DataBuffer>( new draco::DataBuffer());
      uint32_t count = encoder->mesh.num_points();

      draco::GeometryAttribute attribute;
      attribute.Init(attributeType, &*buffer, componentCount, dracoDataType, false, stride, 0);

      auto id = static_cast<uint32_t>(encoder->mesh.AddAttribute(attribute, true, count));
      auto dataBytes = reinterpret_cast<uint8_t *>(data);

      if(flip) {
        switch (dracoDataType) {
          case draco::DataType::DT_INT8:
            SetAttributeValuesFlipped<T,int8_t>(encoder, id, count, componentCount, stride, dataBytes);
            break;
          case draco::DataType::DT_UINT8:
            SetAttributeValuesFlipped<T,uint8_t>(encoder, id, count, componentCount, stride, dataBytes);
            break;
          case draco::DataType::DT_INT16:
            SetAttributeValuesFlipped<T,int16_t>(encoder, id, count, componentCount, stride, dataBytes);
            break;
          case draco::DataType::DT_UINT16:
            SetAttributeValuesFlipped<T,uint16_t>(encoder, id, count, componentCount, stride, dataBytes);
            break;
          case draco::DataType::DT_INT32:
            SetAttributeValuesFlipped<T,int32_t>(encoder, id, count, componentCount, stride, dataBytes);
            break;
          case draco::DataType::DT_UINT32:
            SetAttributeValuesFlipped<T,uint32_t>(encoder, id, count, componentCount, stride, dataBytes);
            break;
          case draco::DataType::DT_FLOAT32:
            SetAttributeValuesFlipped<T,float>(encoder, id, count, componentCount, stride, dataBytes);
            break;
          default:
            break;
        }
      } else {
        for (uint32_t i = 0; i < count; i++)
        {
            encoder->mesh.attribute(id)->SetAttributeValue(draco::AttributeValueIndex(i), dataBytes + i * stride);
        }
      }

      encoder->buffers.emplace_back(std::move(buffer));
      encoder->rawSize += count * stride;
      return id;
  }

}  // namespace draco

#endif  // DRACO_UNITY_PLUGIN
