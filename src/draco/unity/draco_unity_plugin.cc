// Copyright 2017 The Draco Authors.
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
#include "draco/unity/draco_unity_plugin.h"

#ifdef DRACO_UNITY_PLUGIN

namespace {
// Returns a DracoAttribute from a PointAttribute.
draco::DracoAttribute *CreateDracoAttribute(const draco::PointAttribute *attr) {
  draco::DracoAttribute *const attribute = new draco::DracoAttribute();
  attribute->attribute_type =
      static_cast<draco::GeometryAttribute::Type>(attr->attribute_type());
  attribute->data_type = static_cast<draco::DataType>(attr->data_type());
  attribute->num_components = attr->num_components();
  attribute->unique_id = attr->unique_id();
  attribute->private_attribute = static_cast<const void *>(attr);
  return attribute;
}

// Returns the attribute data in |attr| as an array of type T.
template <typename T>
T *CopyAttributeData(int num_points, const draco::PointAttribute *attr) {
  const int num_components = attr->num_components();
  T *const data = new T[num_points * num_components];

  for (draco::PointIndex i(0); i < num_points; ++i) {
    const draco::AttributeValueIndex val_index = attr->mapped_index(i);
    bool got_data = false;
    switch (num_components) {
      case 1:
        got_data = attr->ConvertValue<T, 1>(val_index,
                                            data + i.value() * num_components);
        break;
      case 2:
        got_data = attr->ConvertValue<T, 2>(val_index,
                                            data + i.value() * num_components);
        break;
      case 3:
        got_data = attr->ConvertValue<T, 3>(val_index,
                                            data + i.value() * num_components);
        break;
      case 4:
        got_data = attr->ConvertValue<T, 4>(val_index,
                                            data + i.value() * num_components);
        break;
      default:
        break;
    }
    if (!got_data) {
      delete[] data;
      return nullptr;
    }
  }

  return data;
}

// Returns the attribute data in |attr| as an array of type T.
template <typename T>
T *CopyAttributeDataFlipped(int num_points, const draco::PointAttribute *attr) {
  const int num_components = attr->num_components();
  T *const data = new T[num_points * num_components];

  for (draco::PointIndex i(0); i < num_points; ++i) {
    const draco::AttributeValueIndex val_index = attr->mapped_index(i);
    bool got_data = false;
    T* outPtr = data + i.value() * num_components;
    switch (num_components) {
      case 1:
        got_data = attr->ConvertValue<T, 1>(val_index,outPtr);
        break;
      case 2:
        got_data = attr->ConvertValue<T, 2>(val_index,outPtr);
        // Texture coordinate right-handed top left to left-handed lower left conversion
        *(outPtr+1) = 1-*(outPtr+1);
        break;
      case 3:
        got_data = attr->ConvertValue<T, 3>(val_index,outPtr);
        // Position/Normal right-hand to left-handed coordinate system switch: flip X axis
        *(outPtr) *= -1;
        break;
      case 4:
        got_data = attr->ConvertValue<T, 4>(val_index,outPtr);
        // Tangent right-hand to left-handed coordinate system switch: flip Y and Z axis
        *(outPtr+1) *= -1;
        *(outPtr+2) *= -1;
        break;
      default:
        break;
    }
    if (!got_data) {
      delete[] data;
      return nullptr;
    }
  }

  return data;
}

// Returns the attribute data in |attr| as an array of void*.
void *ConvertAttributeData(int num_points, const draco::PointAttribute *attr, bool flip) {
  switch (attr->data_type()) {
    case draco::DataType::DT_INT8:
      return static_cast<void *>(flip ? CopyAttributeDataFlipped<int8_t>(num_points, attr) : CopyAttributeData<int8_t>(num_points, attr));
    case draco::DataType::DT_UINT8:
      return static_cast<void *>(flip ? CopyAttributeDataFlipped<uint8_t>(num_points, attr) : CopyAttributeData<uint8_t>(num_points, attr));
    case draco::DataType::DT_INT16:
      return static_cast<void *>(flip ? CopyAttributeDataFlipped<int16_t>(num_points, attr) : CopyAttributeData<int16_t>(num_points, attr));
    case draco::DataType::DT_UINT16:
      return static_cast<void *>(flip ? CopyAttributeDataFlipped<uint16_t>(num_points, attr) : CopyAttributeData<uint16_t>(num_points, attr));
    case draco::DataType::DT_INT32:
      return static_cast<void *>(flip ? CopyAttributeDataFlipped<int32_t>(num_points, attr) : CopyAttributeData<int32_t>(num_points, attr));
    case draco::DataType::DT_UINT32:
      return static_cast<void *>(flip ? CopyAttributeDataFlipped<uint32_t>(num_points, attr) : CopyAttributeData<uint32_t>(num_points, attr));
    case draco::DataType::DT_FLOAT32:
      return static_cast<void *>(flip ? CopyAttributeDataFlipped<float>(num_points, attr) : CopyAttributeData<float>(num_points, attr));
    default:
      return nullptr;
  }
}

bool GetMeshIndicesUInt16(const draco::DracoMesh *mesh, void* indices, uint32_t indicesCount, bool flip) {

  const draco::Mesh *const m = static_cast<const draco::Mesh *>(mesh->private_mesh);
  if(indicesCount < m->num_faces() * 3) {
    return false;
  }
  uint16_t *const temp_indices = static_cast<uint16_t *const>(indices);
  
  if(flip) {
    for (draco::FaceIndex face_id(0); face_id < m->num_faces(); ++face_id) {
      const draco::Mesh::Face &face = m->face(draco::FaceIndex(face_id));
      uint16_t* const dest = temp_indices + face_id.value() * 3;
      const int32_t* src = reinterpret_cast<const int32_t *>(face.data());
      *dest = *src;
      *(dest+1) = *(src+2);
      *(dest+2) = *(src+1);
    }
  } else {
    for (draco::FaceIndex face_id(0); face_id < m->num_faces(); ++face_id) {
      const draco::Mesh::Face &face = m->face(draco::FaceIndex(face_id));
      uint16_t* const dest = temp_indices + face_id.value() * 3;
      const int32_t* src = reinterpret_cast<const int32_t *>(face.data());
      *dest = *src;
      *(dest+1) = *(src+1);
      *(dest+2) = *(src+2);
    } 
  }
  return true;
}

bool GetMeshIndicesUInt32(const draco::DracoMesh *mesh, void* indices, uint32_t indicesCount, bool flip) {

  const draco::Mesh *const m = static_cast<const draco::Mesh *>(mesh->private_mesh);
  if(indicesCount < m->num_faces() * 3) {
    return false;
  }
  uint32_t *const temp_indices = static_cast<uint32_t *const>(indices);

  if(flip) {
    for (draco::FaceIndex face_id(0); face_id < m->num_faces(); ++face_id) {
      const draco::Mesh::Face &face = m->face(draco::FaceIndex(face_id));
      uint32_t* const dest = temp_indices + face_id.value() * 3;
      const int32_t* src = reinterpret_cast<const int32_t *>(face.data());
      *dest = *src;
      *(dest+1) = *(src+2);
      *(dest+2) = *(src+1);
    }
  } else {
    for (draco::FaceIndex face_id(0); face_id < m->num_faces(); ++face_id) {
      const draco::Mesh::Face &face = m->face(draco::FaceIndex(face_id));
      memcpy(temp_indices + face_id.value() * 3,
           reinterpret_cast<const uint32_t *>(face.data()), sizeof(uint32_t) * 3);
    }
  }
  return true;
}
}  // namespace

namespace draco {

void EXPORT_API ReleaseDracoMesh(DracoMesh **mesh_ptr) {
  if (!mesh_ptr) {
    return;
  }
  const DracoMesh *const mesh = *mesh_ptr;
  if (!mesh) {
    return;
  }
  const Mesh *const m = static_cast<const Mesh *>(mesh->private_mesh);
  delete m;
  delete mesh;
  *mesh_ptr = nullptr;
}

void EXPORT_API ReleaseDracoAttribute(DracoAttribute **attr_ptr) {
  if (!attr_ptr) {
    return;
  }
  const DracoAttribute *const attr = *attr_ptr;
  if (!attr) {
    return;
  }
  delete attr;
  *attr_ptr = nullptr;
}

void EXPORT_API ReleaseDracoData(DracoData **data_ptr) {
  if (!data_ptr) {
    return;
  }
  const DracoData *const data = *data_ptr;
  switch (data->data_type) {
    case draco::DataType::DT_INT8:
      delete[] static_cast<int8_t *>(data->data);
      break;
    case draco::DataType::DT_UINT8:
      delete[] static_cast<uint8_t *>(data->data);
      break;
    case draco::DataType::DT_INT16:
      delete[] static_cast<int16_t *>(data->data);
      break;
    case draco::DataType::DT_UINT16:
      delete[] static_cast<uint16_t *>(data->data);
      break;
    case draco::DataType::DT_INT32:
      delete[] static_cast<int32_t *>(data->data);
      break;
    case draco::DataType::DT_UINT32:
      delete[] static_cast<uint32_t *>(data->data);
      break;
    case draco::DataType::DT_FLOAT32:
      delete[] static_cast<float *>(data->data);
      break;
    default:
      break;
  }
  delete data;
  *data_ptr = nullptr;
}

int EXPORT_API DecodeDracoMeshStep1(
                                    char *data,
                                    unsigned int length,
                                    DracoMesh **mesh,
                                    draco::Decoder** decoder,
                                    draco::DecoderBuffer** buffer
                                    )
{
  if (mesh == nullptr || *mesh != nullptr) {
    return -1;
  }
  *buffer = new draco::DecoderBuffer();
  (*buffer)->Init(data, length);
  auto type_statusor = draco::Decoder::GetEncodedGeometryType(*buffer);
  if (!type_statusor.ok()) {
    // TODO(draco-eng): Use enum instead.
    return -2;
  }
  const draco::EncodedGeometryType geom_type = type_statusor.value();
  if (geom_type != draco::TRIANGULAR_MESH && geom_type != draco::POINT_CLOUD) {
    return -3;
  }

  *mesh = new DracoMesh();
  *decoder = new draco::Decoder();

  if (geom_type == draco::TRIANGULAR_MESH) {
    auto statusor = (*decoder)->DecodeMeshFromBufferStep1(*buffer);
    if (!statusor.ok()) {
      return -4;
    }

    std::unique_ptr<draco::Mesh> in_mesh = std::move(statusor).value();
    DracoMesh *const unity_mesh = *mesh;
    unity_mesh->num_faces = in_mesh->num_faces();
    unity_mesh->num_vertices = in_mesh->num_points();
    unity_mesh->num_attributes = in_mesh->num_attributes();
    unity_mesh->private_mesh = static_cast<void *>(in_mesh.release());

  } else if (geom_type == draco::POINT_CLOUD) {
    auto statusor = (*decoder)->DecodePointCloudFromBuffer(*buffer);
    if (!statusor.ok()) {
      return -4;
    }

    std::unique_ptr<draco::PointCloud> in_cloud = std::move(statusor).value();
    DracoMesh *const unity_mesh = *mesh;
    unity_mesh->num_faces = 0;
    unity_mesh->num_vertices = in_cloud->num_points();
    unity_mesh->num_attributes = in_cloud->num_attributes();
    unity_mesh->private_mesh = static_cast<void *>(in_cloud.release());
  }
  return 0;
}

int EXPORT_API DecodeDracoMeshStep2(DracoMesh **mesh,draco::Decoder* decoder, draco::DecoderBuffer* buffer) {
  auto status = decoder->DecodeMeshFromBufferStep2();
  delete decoder;
  delete buffer;
  if (!status.ok()) {
    return -4;
  }
  return 0;
}

bool EXPORT_API GetAttribute(const DracoMesh *mesh, int index,
                             DracoAttribute **attribute) {
  if (mesh == nullptr || attribute == nullptr || *attribute != nullptr) {
    return false;
  }
  const Mesh *const m = static_cast<const Mesh *>(mesh->private_mesh);
  const PointAttribute *const attr = m->attribute(index);
  if (attr == nullptr) {
    return false;
  }

  *attribute = CreateDracoAttribute(attr);
  return true;
}

bool EXPORT_API GetAttributeByType(const DracoMesh *mesh,
                                   GeometryAttribute::Type type, int index,
                                   DracoAttribute **attribute) {
  if (mesh == nullptr || attribute == nullptr || *attribute != nullptr) {
    return false;
  }
  const Mesh *const m = static_cast<const Mesh *>(mesh->private_mesh);
  GeometryAttribute::Type att_type = static_cast<GeometryAttribute::Type>(type);
  const PointAttribute *const attr = m->GetNamedAttribute(att_type, index);
  if (attr == nullptr) {
    return false;
  }
  *attribute = CreateDracoAttribute(attr);
  return true;
}

bool EXPORT_API GetAttributeByUniqueId(const DracoMesh *mesh, int unique_id,
                                       DracoAttribute **attribute) {
  if (mesh == nullptr || attribute == nullptr || *attribute != nullptr) {
    return false;
  }
  const Mesh *const m = static_cast<const Mesh *>(mesh->private_mesh);
  const PointAttribute *const attr = m->GetAttributeByUniqueId(unique_id);
  if (attr == nullptr) {
    return false;
  }
  *attribute = CreateDracoAttribute(attr);
  return true;
}

bool EXPORT_API GetMeshIndices(const DracoMesh *mesh, DataType dataType, void* indices, uint32_t indicesCount, bool flip) {
  if (mesh == nullptr || indices == nullptr) {
    return false;
  }
  
  void *tmp;
  switch(dataType) {
    case DT_UINT16:
      return GetMeshIndicesUInt16(mesh,indices,indicesCount,flip);
    case DT_UINT32:
      return GetMeshIndicesUInt32(mesh,indices,indicesCount,flip);
    default:
      return false;
  }

  return true;
}

bool EXPORT_API GetAttributeData(const DracoMesh *mesh,
                                 const DracoAttribute *attribute,
                                 DracoData **data, bool flip) {
  if (mesh == nullptr || data == nullptr || *data != nullptr) {
    return false;
  }
  const Mesh *const m = static_cast<const Mesh *>(mesh->private_mesh);
  const PointAttribute *const attr =
      static_cast<const PointAttribute *>(attribute->private_attribute);

  void *temp_data = ConvertAttributeData(m->num_points(), attr, flip);
  if (temp_data == nullptr) {
    return false;
  }
  DracoData *const draco_data = new DracoData();
  draco_data->data = temp_data;
  draco_data->data_type = static_cast<DataType>(attr->data_type());
  *data = draco_data;
  return true;
}

void ReleaseUnityMesh(DracoToUnityMesh **mesh_ptr) {
  DracoToUnityMesh *mesh = *mesh_ptr;
  if (!mesh) {
    return;
  }
  if (mesh->indices) {
    delete[] mesh->indices;
    mesh->indices = nullptr;
  }
  if (mesh->position) {
    delete[] mesh->position;
    mesh->position = nullptr;
  }
  if (mesh->has_normal && mesh->normal) {
    delete[] mesh->normal;
    mesh->has_normal = false;
    mesh->normal = nullptr;
  }
  if (mesh->has_texcoord && mesh->texcoord) {
    delete[] mesh->texcoord;
    mesh->has_texcoord = false;
    mesh->texcoord = nullptr;
  }
  if (mesh->has_color && mesh->color) {
    delete[] mesh->color;
    mesh->has_color = false;
    mesh->color = nullptr;
  }
  delete mesh;
  *mesh_ptr = nullptr;
}

int DecodeMeshForUnity(char *data, unsigned int length,
                       DracoToUnityMesh **tmp_mesh) {
  draco::DecoderBuffer buffer;
  buffer.Init(data, length);
  auto type_statusor = draco::Decoder::GetEncodedGeometryType(&buffer);
  if (!type_statusor.ok()) {
    // TODO(draco-eng): Use enum instead.
    return -1;
  }
  const draco::EncodedGeometryType geom_type = type_statusor.value();
  if (geom_type != draco::TRIANGULAR_MESH) {
    return -2;
  }

  draco::Decoder decoder;
  auto statusor = decoder.DecodeMeshFromBuffer(&buffer);
  if (!statusor.ok()) {
    return -3;
  }
  std::unique_ptr<draco::Mesh> in_mesh = std::move(statusor).value();

  *tmp_mesh = new DracoToUnityMesh();
  DracoToUnityMesh *unity_mesh = *tmp_mesh;
  unity_mesh->num_faces = in_mesh->num_faces();
  unity_mesh->num_vertices = in_mesh->num_points();

  unity_mesh->indices = new int[in_mesh->num_faces() * 3];
  for (draco::FaceIndex face_id(0); face_id < in_mesh->num_faces(); ++face_id) {
    const Mesh::Face &face = in_mesh->face(draco::FaceIndex(face_id));
    memcpy(unity_mesh->indices + face_id.value() * 3,
           reinterpret_cast<const int *>(face.data()), sizeof(int) * 3);
  }

  // TODO(draco-eng): Add other attributes.
  unity_mesh->position = new float[in_mesh->num_points() * 3];
  const auto pos_att =
      in_mesh->GetNamedAttribute(draco::GeometryAttribute::POSITION);
  for (draco::PointIndex i(0); i < in_mesh->num_points(); ++i) {
    const draco::AttributeValueIndex val_index = pos_att->mapped_index(i);
    if (!pos_att->ConvertValue<float, 3>(
            val_index, unity_mesh->position + i.value() * 3)) {
      ReleaseUnityMesh(&unity_mesh);
      return -8;
    }
  }
  // Get normal attributes.
  const auto normal_att =
      in_mesh->GetNamedAttribute(draco::GeometryAttribute::NORMAL);
  if (normal_att != nullptr) {
    unity_mesh->normal = new float[in_mesh->num_points() * 3];
    unity_mesh->has_normal = true;
    for (draco::PointIndex i(0); i < in_mesh->num_points(); ++i) {
      const draco::AttributeValueIndex val_index = normal_att->mapped_index(i);
      if (!normal_att->ConvertValue<float, 3>(
              val_index, unity_mesh->normal + i.value() * 3)) {
        ReleaseUnityMesh(&unity_mesh);
        return -8;
      }
    }
  }
  // Get color attributes.
  const auto color_att =
      in_mesh->GetNamedAttribute(draco::GeometryAttribute::COLOR);
  if (color_att != nullptr) {
    unity_mesh->color = new float[in_mesh->num_points() * 4];
    unity_mesh->has_color = true;
    for (draco::PointIndex i(0); i < in_mesh->num_points(); ++i) {
      const draco::AttributeValueIndex val_index = color_att->mapped_index(i);
      if (!color_att->ConvertValue<float, 4>(
              val_index, unity_mesh->color + i.value() * 4)) {
        ReleaseUnityMesh(&unity_mesh);
        return -8;
      }
      if (color_att->num_components() < 4) {
        // If the alpha component wasn't set in the input data we should set
        // it to an opaque value.
        unity_mesh->color[i.value() * 4 + 3] = 1.f;
      }
    }
  }
  // Get texture coordinates attributes.
  const auto texcoord_att =
      in_mesh->GetNamedAttribute(draco::GeometryAttribute::TEX_COORD);
  if (texcoord_att != nullptr) {
    unity_mesh->texcoord = new float[in_mesh->num_points() * 2];
    unity_mesh->has_texcoord = true;
    for (draco::PointIndex i(0); i < in_mesh->num_points(); ++i) {
      const draco::AttributeValueIndex val_index =
          texcoord_att->mapped_index(i);
      if (!texcoord_att->ConvertValue<float, 2>(
              val_index, unity_mesh->texcoord + i.value() * 2)) {
        ReleaseUnityMesh(&unity_mesh);
        return -8;
      }
    }
  }

  return in_mesh->num_faces();
}

}  // namespace draco

#endif  // DRACO_UNITY_PLUGIN
