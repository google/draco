// Copyright 2018 The Draco Authors.
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
#include "draco/io/gltf_decoder.h"

#ifdef DRACO_TRANSCODER_SUPPORTED
#include <array>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <vector>

#include "draco/attributes/geometry_attribute.h"
#include "draco/attributes/geometry_indices.h"
#include "draco/core/decoder_buffer.h"
#include "draco/core/draco_types.h"
#include "draco/core/hash_utils.h"
#include "draco/core/status.h"
#include "draco/core/status_or.h"
#include "draco/core/vector_d.h"
#include "draco/io/file_utils.h"
#include "draco/io/texture_io.h"
#include "draco/io/tiny_gltf_utils.h"
#include "draco/material/material.h"
#include "draco/material/material_library.h"
#include "draco/mesh/mesh.h"
#include "draco/mesh/mesh_features.h"
#include "draco/mesh/triangle_soup_mesh_builder.h"
#include "draco/metadata/geometry_metadata.h"
#include "draco/metadata/json_entity.h"
#include "draco/metadata/metadata.h"
#include "draco/metadata/property_table.h"
#include "draco/metadata/property_texture.h"
#include "draco/metadata/structural_metadata.h"
#include "draco/metadata/structural_metadata_schema.h"
#include "draco/point_cloud/point_cloud_builder.h"
#include "draco/scene/audio.h"
#include "draco/scene/scene_indices.h"
#include "draco/texture/source_image.h"
#include "draco/texture/texture_library.h"
#include "draco/texture/texture_map.h"
#include "draco/texture/texture_utils.h"

#ifdef DRACO_PMC_SUPPORTED
#include "draco/compression/pmc/pmc_draco_mesh_decoder.h"
#endif  // DRACO_PMC_SUPPORTED

namespace draco {

namespace {

constexpr char kDracoMetadataGltfAttributeName[] =
    "//GLTF/ApplicationSpecificAttributeName";

draco::DataType GltfComponentTypeToDracoType(int component_type) {
  switch (component_type) {
    case TINYGLTF_COMPONENT_TYPE_BYTE:
      return DT_INT8;
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
      return DT_UINT8;
    case TINYGLTF_COMPONENT_TYPE_SHORT:
      return DT_INT16;
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
      return DT_UINT16;
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
      return DT_UINT32;
    case TINYGLTF_COMPONENT_TYPE_INT:
      return DT_INT32;
    case TINYGLTF_COMPONENT_TYPE_FLOAT:
      return DT_FLOAT32;
  }
  return DT_INVALID;
}

GeometryAttribute::Type GltfAttributeToDracoAttribute(
    const std::string attribute_name) {
  if (attribute_name == "POSITION") {
    return GeometryAttribute::POSITION;
  } else if (attribute_name == "NORMAL") {
    return GeometryAttribute::NORMAL;
  } else if (attribute_name.rfind("TEXCOORD_", 0) == 0) {
    return GeometryAttribute::TEX_COORD;
  } else if (attribute_name == "TANGENT") {
    return GeometryAttribute::TANGENT;
  } else if (attribute_name == "COLOR_0") {
    return GeometryAttribute::COLOR;
  } else if (attribute_name == "JOINTS_0") {
    return GeometryAttribute::JOINTS;
  } else if (attribute_name == "WEIGHTS_0") {
    return GeometryAttribute::WEIGHTS;
  } else if (attribute_name.rfind("_FEATURE_ID_") == 0) {
    // Feature ID attribute like _FEATURE_ID_5 from EXT_mesh_features extension.
    return GeometryAttribute::GENERIC;
  } else if (attribute_name.rfind('_', 0) == 0) {
    // Feature ID attribute like _DIRECTION from EXT_structural_metadata
    // extension whose name begins with an underscore.
    return GeometryAttribute::GENERIC;
  }
  return GeometryAttribute::INVALID;
}

StatusOr<TextureMap::AxisWrappingMode> TinyGltfToDracoAxisWrappingMode(
    int wrap_mode) {
  switch (wrap_mode) {
    case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE:
      return TextureMap::CLAMP_TO_EDGE;
    case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT:
      return TextureMap::MIRRORED_REPEAT;
    case TINYGLTF_TEXTURE_WRAP_REPEAT:
      return TextureMap::REPEAT;
    default:
      return Status(Status::UNSUPPORTED_FEATURE, "Unsupported wrapping mode.");
  }
}

StatusOr<TextureMap::FilterType> TinyGltfToDracoFilterType(int filter_type) {
  switch (filter_type) {
    case -1:
      return TextureMap::UNSPECIFIED;
    case TINYGLTF_TEXTURE_FILTER_NEAREST:
      return TextureMap::NEAREST;
    case TINYGLTF_TEXTURE_FILTER_LINEAR:
      return TextureMap::LINEAR;
    case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST:
      return TextureMap::NEAREST_MIPMAP_NEAREST;
    case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST:
      return TextureMap::LINEAR_MIPMAP_NEAREST;
    case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR:
      return TextureMap::NEAREST_MIPMAP_LINEAR;
    case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR:
      return TextureMap::LINEAR_MIPMAP_LINEAR;
    default:
      return Status(Status::DRACO_ERROR, "Unsupported texture filter type.");
  }
}

StatusOr<std::vector<uint32_t>> CopyDataAsUint32(
    const tinygltf::Model& model, const tinygltf::Accessor& accessor) {
  if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_BYTE) {
    return Status(Status::DRACO_ERROR, "Byte cannot be converted to Uint32.");
  }
  if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_SHORT) {
    return Status(Status::DRACO_ERROR, "Short cannot be converted to Uint32.");
  }
  if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_INT) {
    return Status(Status::DRACO_ERROR, "Int cannot be converted to Uint32.");
  }
  if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) {
    return Status(Status::DRACO_ERROR, "Float cannot be converted to Uint32.");
  }
  if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_DOUBLE) {
    return Status(Status::DRACO_ERROR, "Double cannot be converted to Uint32.");
  }
  DRACO_RETURN_IF_ERROR(TinyGltfUtils::CheckAccessorBounds(model, accessor));

  const tinygltf::BufferView& buffer_view =
      model.bufferViews[accessor.bufferView];
  const tinygltf::Buffer& buffer = model.buffers[buffer_view.buffer];

  const uint8_t* const data_start =
      buffer.data.data() + buffer_view.byteOffset + accessor.byteOffset;
  const int byte_stride = accessor.ByteStride(buffer_view);
  const int component_size =
      tinygltf::GetComponentSizeInBytes(accessor.componentType);
  const int num_components =
      TinyGltfUtils::GetNumComponentsForType(accessor.type);
  const int num_elements = accessor.count * num_components;

  std::vector<uint32_t> output;
  output.resize(num_elements);

  int out_index = 0;
  const uint8_t* data = data_start;
  for (int i = 0; i < accessor.count; ++i) {
    for (int c = 0; c < num_components; ++c) {
      uint32_t value = 0;
      memcpy(&value, data + (c * component_size), component_size);
      output[out_index++] = value;
    }

    data += byte_stride;
  }

  return output;
}

// Specialization for arithmetic types.
template <typename TypeT,
          std::enable_if_t<std::is_arithmetic_v<TypeT>>* = nullptr>
StatusOr<std::vector<TypeT>> CopyDataAs(const tinygltf::Model& model,
                                        const tinygltf::Accessor& accessor) {
  if (std::is_same_v<TypeT, uint8_t>) {
    if (TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE != accessor.componentType) {
      return ErrorStatus("Accessor data cannot be converted to Uint8.");
    }
  } else if (std::is_same_v<TypeT, uint16_t>) {
    if (TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE != accessor.componentType &&
        TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT != accessor.componentType) {
      return ErrorStatus("Accessor data cannot be converted to Uint16.");
    }
  } else if (std::is_same_v<TypeT, uint32_t>) {
    if (TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE != accessor.componentType &&
        TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT != accessor.componentType &&
        TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT != accessor.componentType) {
      return ErrorStatus("Accessor data cannot be converted to Uint32.");
    }
  } else if (std::is_same_v<TypeT, float>) {
    if (TINYGLTF_COMPONENT_TYPE_FLOAT != accessor.componentType) {
      return ErrorStatus("Accessor data cannot be converted to Float.");
    }
  } else if (std::is_same_v<TypeT, int8_t>) {
    if (TINYGLTF_COMPONENT_TYPE_BYTE != accessor.componentType) {
      return ErrorStatus("Accessor data cannot be converted to Int8.");
    }
  } else if (std::is_same_v<TypeT, int16_t>) {
    if (TINYGLTF_COMPONENT_TYPE_SHORT != accessor.componentType) {
      return ErrorStatus("Accessor data cannot be converted to Int16.");
    }
  } else if (std::is_same_v<TypeT, int32_t>) {
    if (TINYGLTF_COMPONENT_TYPE_INT != accessor.componentType) {
      return ErrorStatus("Accessor data cannot be converted to Int32.");
    }
  }
  DRACO_RETURN_IF_ERROR(TinyGltfUtils::CheckAccessorBounds(model, accessor));

  const tinygltf::BufferView& buffer_view =
      model.bufferViews[accessor.bufferView];
  const tinygltf::Buffer& buffer = model.buffers[buffer_view.buffer];

  const uint8_t* const data_start =
      buffer.data.data() + buffer_view.byteOffset + accessor.byteOffset;
  const int byte_stride = accessor.ByteStride(buffer_view);
  const int component_size =
      tinygltf::GetComponentSizeInBytes(accessor.componentType);

  std::vector<TypeT> output;
  output.resize(accessor.count);

  const int num_components =
      TinyGltfUtils::GetNumComponentsForType(accessor.type);
  int out_index = 0;
  const uint8_t* data = data_start;
  for (int i = 0; i < accessor.count; ++i) {
    for (int c = 0; c < num_components; ++c) {
      TypeT value = 0;
      memcpy(&value, data + (c * component_size), component_size);
      output[out_index++] = value;
    }
    data += byte_stride;
  }
  return output;
}

// Specialization for remaining types is used for draco::VectorD.
template <typename TypeT,
          std::enable_if_t<!std::is_arithmetic_v<TypeT>>* = nullptr>
StatusOr<std::vector<TypeT>> CopyDataAs(const tinygltf::Model& model,
                                        const tinygltf::Accessor& accessor) {
  const int num_components =
      TinyGltfUtils::GetNumComponentsForType(accessor.type);
  if (num_components != TypeT::dimension) {
    return Status(Status::DRACO_ERROR,
                  "Dimension does not equal num components.");
  }
  DRACO_RETURN_IF_ERROR(TinyGltfUtils::CheckAccessorBounds(model, accessor));

  const tinygltf::BufferView& buffer_view =
      model.bufferViews[accessor.bufferView];
  const tinygltf::Buffer& buffer = model.buffers[buffer_view.buffer];

  const uint8_t* const data_start =
      buffer.data.data() + buffer_view.byteOffset + accessor.byteOffset;
  const int byte_stride = accessor.ByteStride(buffer_view);
  const int component_size =
      tinygltf::GetComponentSizeInBytes(accessor.componentType);

  std::vector<TypeT> output;
  output.resize(accessor.count);

  const uint8_t* data = data_start;
  for (int i = 0; i < accessor.count; ++i) {
    TypeT values;
    for (int c = 0; c < num_components; ++c) {
      memcpy(&values[c], data + (c * component_size), component_size);
    }
    output[i] = values;
    data += byte_stride;
  }
  return output;
}

// Copies the data referenced from |buffer_view_id| into |data|. Currently only
// supports a byte stride of 0. I.e. tightly packed.
Status CopyDataFromBufferView(const tinygltf::Model& model, int buffer_view_id,
                              std::vector<uint8_t>* data) {
  if (buffer_view_id < 0) {
    return ErrorStatus("Error CopyDataFromBufferView() bufferView < 0.");
  }
  const tinygltf::BufferView& buffer_view = model.bufferViews[buffer_view_id];
  if (buffer_view.buffer < 0) {
    return ErrorStatus("Error CopyDataFromBufferView() buffer < 0.");
  }
  if (buffer_view.byteStride != 0) {
    return Status(Status::DRACO_ERROR, "Error buffer view byteStride != 0.");
  }

  const tinygltf::Buffer& buffer = model.buffers[buffer_view.buffer];
  const uint8_t* const data_start = buffer.data.data() + buffer_view.byteOffset;

  data->resize(buffer_view.byteLength);
  memcpy(data->data(), data_start, buffer_view.byteLength);
  return OkStatus();
}

// Returns a SourceImage created from |image|.
StatusOr<std::unique_ptr<SourceImage>> GetSourceImage(
    const tinygltf::Model& model, const tinygltf::Image& image,
    const Texture& texture) {
  std::unique_ptr<SourceImage> source_image(new SourceImage());
  // If the image is in an external file then the buffer view is < 0.
  if (image.bufferView >= 0) {
    DRACO_RETURN_IF_ERROR(CopyDataFromBufferView(
        model, image.bufferView, &source_image->MutableEncodedData()));
  }
  if (!image.uri.empty()) {
    source_image->set_filename(image.uri);
  } else if (!image.name.empty()) {
    source_image->set_filename(RemoveFileExtension(image.name) + "." +
                               LowercaseMimeTypeExtension(image.mimeType));
  }
  source_image->set_mime_type(image.mimeType);

  return source_image;
}

std::unique_ptr<TrsMatrix> GetNodeTrsMatrix(const tinygltf::Node& node) {
  std::unique_ptr<TrsMatrix> trsm(new TrsMatrix());
  if (node.matrix.size() == 16) {
    Eigen::Matrix4d transformation;
    // clang-format off
    // |node.matrix| is in the column-major order.
    transformation <<
        node.matrix[0],  node.matrix[4],  node.matrix[8],  node.matrix[12],
        node.matrix[1],  node.matrix[5],  node.matrix[9],  node.matrix[13],
        node.matrix[2],  node.matrix[6],  node.matrix[10], node.matrix[14],
        node.matrix[3],  node.matrix[7],  node.matrix[11], node.matrix[15];
    // clang-format on
    if (transformation != Eigen::Matrix4d::Identity()) {
      trsm->SetMatrix(transformation);
    }
  }

  if (node.translation.size() == 3) {
    const Eigen::Vector3d default_translation(0.0, 0.0, 0.0);
    const Eigen::Vector3d node_translation(
        node.translation[0], node.translation[1], node.translation[2]);
    if (node_translation != default_translation) {
      trsm->SetTranslation(node_translation);
    }
  }
  if (node.scale.size() == 3) {
    const Eigen::Vector3d default_scale(1.0, 1.0, 1.0);
    const Eigen::Vector3d node_scale(node.scale[0], node.scale[1],
                                     node.scale[2]);
    if (node_scale != default_scale) {
      trsm->SetScale(node_scale);
    }
  }
  if (node.rotation.size() == 4) {
    // Eigen quaternion is defined in (w, x, y, z) vs glTF that uses
    // (x, y, z, w).
    const Eigen::Quaterniond default_rotation(0.0, 0.0, 0.0, 1.0);
    const Eigen::Quaterniond node_rotation(node.rotation[3], node.rotation[0],
                                           node.rotation[1], node.rotation[2]);
    if (node_rotation != default_rotation) {
      trsm->SetRotation(node_rotation);
    }
  }

  return trsm;
}

Eigen::Matrix4d UpdateMatrixForNormals(
    const Eigen::Matrix4d& transform_matrix) {
  Eigen::Matrix3d mat3x3;
  // clang-format off
  mat3x3 <<
      transform_matrix(0, 0), transform_matrix(0, 1), transform_matrix(0, 2),
      transform_matrix(1, 0), transform_matrix(1, 1), transform_matrix(1, 2),
      transform_matrix(2, 0), transform_matrix(2, 1), transform_matrix(2, 2);
  // clang-format on

  mat3x3 = mat3x3.inverse().transpose();
  Eigen::Matrix4d mat4x4;
  // clang-format off
  mat4x4 << mat3x3(0, 0), mat3x3(0, 1), mat3x3(0, 2), 0.0,
            mat3x3(1, 0), mat3x3(1, 1), mat3x3(1, 2), 0.0,
            mat3x3(2, 0), mat3x3(2, 1), mat3x3(2, 2), 0.0,
            0.0,          0.0,          0.0,          1.0;
  // clang-format on
  return mat4x4;
}

float Determinant(const Eigen::Matrix4d& transform_matrix) {
  Eigen::Matrix3d mat3x3;
  // clang-format off
  mat3x3 <<
      transform_matrix(0, 0), transform_matrix(0, 1), transform_matrix(0, 2),
      transform_matrix(1, 0), transform_matrix(1, 1), transform_matrix(1, 2),
      transform_matrix(2, 0), transform_matrix(2, 1), transform_matrix(2, 2);
  // clang-format on
  return mat3x3.determinant();
}

bool FileExists(const std::string& filepath, void* /*user_data*/) {
  return GetFileSize(filepath) != 0;
}

bool ReadWholeFile(std::vector<unsigned char>* out, std::string* err,
                   const std::string& filepath, void* user_data) {
  if (!ReadFileToBuffer(filepath, out)) {
    if (err) {
      *err = "Unable to read: " + filepath;
    }
    return false;
  }
  if (user_data) {
    auto* files_vector = reinterpret_cast<std::vector<std::string>*>(user_data);
    files_vector->push_back(filepath);
  }
  return true;
}

bool WriteWholeFile(std::string* /*err*/, const std::string& filepath,
                    const std::vector<unsigned char>& contents,
                    void* /*user_data*/) {
  return WriteBufferToFile(contents.data(), contents.size(), filepath);
}

template <typename BuilderT>
void AddAppSpecificAttrName(int att_id, const std::string& name,
                            BuilderT* builder) {
  if (name.find("_FEATURE_ID_") == 0) {
    // _FEATURE_ID_n attributes are handled specially in the encoder.
    return;
  }

  // Ensure we round-trip GENERIC attributes by saving their names.
  auto att_metadata = std::make_unique<draco::AttributeMetadata>();
  att_metadata->AddEntryString(kDracoMetadataGltfAttributeName, name);
  builder->AddAttributeMetadata(att_id, std::move(att_metadata));
}

}  // namespace

GltfDecoder::GltfDecoder()
    : next_face_id_(0),
      next_point_id_(0),
      total_face_indices_count_(0),
      total_point_indices_count_(0),
      material_att_id_(-1) {}

StatusOr<std::unique_ptr<Mesh>> GltfDecoder::DecodeFromFile(
    const std::string& file_name) {
  return DecodeFromFile(file_name, nullptr);
}

StatusOr<std::unique_ptr<Mesh>> GltfDecoder::DecodeFromFile(
    const std::string& file_name, std::vector<std::string>* mesh_files) {
  DRACO_RETURN_IF_ERROR(LoadFile(file_name, mesh_files));
  return BuildMesh();
}

StatusOr<std::unique_ptr<Mesh>> GltfDecoder::DecodeFromBuffer(
    DecoderBuffer* buffer) {
  DRACO_RETURN_IF_ERROR(LoadBuffer(*buffer));
  return BuildMesh();
}

StatusOr<std::unique_ptr<Scene>> GltfDecoder::DecodeFromFileToScene(
    const std::string& file_name) {
  return DecodeFromFileToScene(file_name, nullptr);
}

StatusOr<std::unique_ptr<Scene>> GltfDecoder::DecodeFromFileToScene(
    const std::string& file_name, std::vector<std::string>* scene_files) {
  DRACO_RETURN_IF_ERROR(LoadFile(file_name, scene_files));
  scene_ = std::unique_ptr<Scene>(new Scene());
  DRACO_RETURN_IF_ERROR(DecodeGltfToScene());
  return std::move(scene_);
}

StatusOr<std::unique_ptr<Scene>> GltfDecoder::DecodeFromBufferToScene(
    DecoderBuffer* buffer) {
  DRACO_RETURN_IF_ERROR(LoadBuffer(*buffer));
  scene_ = std::unique_ptr<Scene>(new Scene());
  DRACO_RETURN_IF_ERROR(DecodeGltfToScene());
  return std::move(scene_);
}

Status GltfDecoder::LoadFile(const std::string& file_name,
                             std::vector<std::string>* input_files) {
  const std::string extension = LowercaseFileExtension(file_name);
  tinygltf::TinyGLTF loader;
  std::string err;
  std::string warn;

  tinygltf::FsCallbacks fs_callbacks = {};
  fs_callbacks.FileExists = &FileExists;
  // TinyGLTF's ExpandFilePath does not do filesystem i/o, so it's safe to
  // use in all environments.
  fs_callbacks.ExpandFilePath = &tinygltf::ExpandFilePath;
  fs_callbacks.ReadWholeFile = &ReadWholeFile;
  fs_callbacks.WriteWholeFile = &WriteWholeFile;
  fs_callbacks.user_data = reinterpret_cast<void*>(input_files);

  loader.SetFsCallbacks(fs_callbacks);

  if (extension == "glb") {
    if (!loader.LoadBinaryFromFile(&gltf_model_, &err, &warn, file_name)) {
      return Status(Status::DRACO_ERROR,
                    "TinyGLTF failed to load glb file: " + err);
    }
  } else if (extension == "gltf") {
    if (!loader.LoadASCIIFromFile(&gltf_model_, &err, &warn, file_name)) {
      return Status(Status::DRACO_ERROR,
                    "TinyGLTF failed to load glTF file: " + err);
    }
  } else {
    return Status(Status::DRACO_ERROR, "Unknown input file extension.");
  }
  DRACO_RETURN_IF_ERROR(CheckUnsupportedFeatures());
  input_file_name_ = file_name;
  return OkStatus();
}

Status GltfDecoder::LoadBuffer(const DecoderBuffer& buffer) {
  tinygltf::TinyGLTF loader;
  std::string err;
  std::string warn;

  if (!loader.LoadBinaryFromMemory(
          &gltf_model_, &err, &warn,
          reinterpret_cast<const unsigned char*>(buffer.data_head()),
          buffer.remaining_size())) {
    return Status(Status::DRACO_ERROR,
                  "TinyGLTF failed to load glb buffer: " + err);
  }
  DRACO_RETURN_IF_ERROR(CheckUnsupportedFeatures());
  input_file_name_.clear();
  return OkStatus();
}

StatusOr<std::unique_ptr<Mesh>> GltfDecoder::BuildMesh() {
  DRACO_RETURN_IF_ERROR(GatherAttributeAndMaterialStats());
  if (total_face_indices_count_ > 0 && total_point_indices_count_ > 0) {
    return ErrorStatus(
        "Decoding to mesh can't handle triangle and point primitives at the "
        "same time.");
  }
  if (total_face_indices_count_ > 0) {
    mb_.Start(total_face_indices_count_ / 3);
    DRACO_RETURN_IF_ERROR(AddAttributesToDracoMesh(&mb_));
  } else {
    pb_.Start(total_point_indices_count_);
    DRACO_RETURN_IF_ERROR(AddAttributesToDracoMesh(&pb_));
  }

  // Clear attribute indices before populating attributes in |mb_| or |pb_|.
  feature_id_attribute_indices_.clear();

  for (const tinygltf::Scene& scene : gltf_model_.scenes) {
    for (int i = 0; i < scene.nodes.size(); ++i) {
      const Eigen::Matrix4d parent_matrix = Eigen::Matrix4d::Identity();
      DRACO_RETURN_IF_ERROR(DecodeNode(scene.nodes[i], parent_matrix));
    }
  }
  DRACO_ASSIGN_OR_RETURN(
      std::unique_ptr<Mesh> mesh,
      BuildMeshFromBuilder(total_face_indices_count_ > 0, &mb_, &pb_,
                           deduplicate_vertices_));

  DRACO_RETURN_IF_ERROR(CopyTextures<Mesh>(mesh.get()));
  SetAttributePropertiesOnDracoMesh(mesh.get());
  DRACO_RETURN_IF_ERROR(AddMaterialsToDracoMesh(mesh.get()));
  DRACO_RETURN_IF_ERROR(AddPrimitiveExtensionsToDracoMesh(mesh.get()));
  DRACO_RETURN_IF_ERROR(AddStructuralMetadataToGeometry(mesh.get()));
  MoveNonMaterialTextures(mesh.get());
  DRACO_RETURN_IF_ERROR(AddAssetMetadata(mesh.get()));
  return mesh;
}

Status GltfDecoder::AddPrimitiveExtensionsToDracoMesh(Mesh* mesh) {
  for (const tinygltf::Scene& scene : gltf_model_.scenes) {
    for (int i = 0; i < scene.nodes.size(); ++i) {
      DRACO_RETURN_IF_ERROR(
          AddPrimitiveExtensionsToDracoMesh(scene.nodes[i], mesh));
    }
  }
  return OkStatus();
}

Status GltfDecoder::AddPrimitiveExtensionsToDracoMesh(int node_index,
                                                      Mesh* mesh) {
  const tinygltf::Node& node = gltf_model_.nodes[node_index];
  if (node.mesh >= 0) {
    const tinygltf::Mesh& gltf_mesh = gltf_model_.meshes[node.mesh];
    for (const auto& primitive : gltf_mesh.primitives) {
      // Decode extensions present in this primitive.
      DRACO_RETURN_IF_ERROR(AddPrimitiveExtensionsToDracoMesh(
          primitive, &mesh->GetMaterialLibrary().MutableTextureLibrary(),
          mesh));
    }
  }
  for (int i = 0; i < node.children.size(); ++i) {
    DRACO_RETURN_IF_ERROR(
        AddPrimitiveExtensionsToDracoMesh(node.children[i], mesh));
  }
  return OkStatus();
}

Status GltfDecoder::AddPrimitiveExtensionsToDracoMesh(
    const tinygltf::Primitive& primitive, TextureLibrary* texture_library,
    Mesh* mesh) {
  // Decode mesh feature ID sets if present in this |primitive|.
  DRACO_RETURN_IF_ERROR(DecodeMeshFeatures(primitive, texture_library, mesh));

  // Decode structural metadata if present in this |primitive|.
  DRACO_RETURN_IF_ERROR(DecodeStructuralMetadata(primitive, mesh));

  return OkStatus();
}

Status GltfDecoder::CheckUnsupportedFeatures() {
  // Check for sparse accessors.
  for (const auto& accessor : gltf_model_.accessors) {
    if (accessor.sparse.isSparse) {
      return Status(Status::UNSUPPORTED_FEATURE,
                    "Sparse accessors are unsupported.");
    }
  }

  // Check for extensions.
  for (const auto& extension : gltf_model_.extensionsRequired) {
    if (extension != "KHR_materials_unlit" &&
        extension != "KHR_texture_transform" &&
        extension != "KHR_draco_mesh_compression" &&
        extension != "KHR_pmc_mesh_compression" &&
        extension != "KHR_interactivity" && extension != "GOOG_mask" &&
        extension != "KHR_materials_variants" &&
        extension != "KHR_materials_transmission" &&
        extension != "KHR_lights_punctual" &&
        extension != "KHR_materials_clearcoat" &&
        extension != "KHR_materials_sheen" &&
        extension != "KHR_materials_ior" &&
        extension != "KHR_materials_specular" &&
        extension != "KHR_materials_emissive_strength" &&
        extension != "KHR_animation_pointer") {
      return Status(Status::UNSUPPORTED_FEATURE,
                    extension + " is unsupported.");
    }
  }
  return OkStatus();
}

Status GltfDecoder::DecodeNode(int node_index,
                               const Eigen::Matrix4d& parent_matrix) {
  if (node_index < 0 || node_index >= gltf_model_.nodes.size()) {
    return Status(Status::DRACO_ERROR, "Node index out of bounds.");
  }
  const tinygltf::Node& node = gltf_model_.nodes[node_index];
  const std::unique_ptr<TrsMatrix> trsm = GetNodeTrsMatrix(node);
  const Eigen::Matrix4d node_matrix =
      parent_matrix * trsm->ComputeTransformationMatrix();

  if (node.mesh >= 0) {
    if (node.mesh >= gltf_model_.meshes.size()) {
      return Status(Status::DRACO_ERROR, "Mesh index out of bounds.");
    }
    const tinygltf::Mesh& mesh = gltf_model_.meshes[node.mesh];
    for (const auto& primitive : mesh.primitives) {
      DRACO_RETURN_IF_ERROR(DecodePrimitive(primitive, node_matrix));
    }
  }
  for (int i = 0; i < node.children.size(); ++i) {
    DRACO_RETURN_IF_ERROR(DecodeNode(node.children[i], node_matrix));
  }
  return OkStatus();
}

StatusOr<int> GltfDecoder::DecodePrimitiveAttributeCount(
    const tinygltf::Primitive& primitive) const {
  // Use the first primitive attribute as all attributes have the same entry
  // count according to glTF 2.0 spec.
  if (primitive.attributes.empty()) {
    return Status(Status::DRACO_ERROR, "Primitive has no attributes.");
  }
  const tinygltf::Accessor& accessor =
      gltf_model_.accessors[primitive.attributes.begin()->second];
  return accessor.count;
}

StatusOr<int> GltfDecoder::DecodePrimitiveIndicesCount(
    const tinygltf::Primitive& primitive) const {
  if (primitive.indices < 0) {
    // Primitive has implicit indices [0, 1, 2, 3, ...]. Determine indices count
    // based on entry count of a primitive attribute.
    return DecodePrimitiveAttributeCount(primitive);
  }
  const tinygltf::Accessor& indices = gltf_model_.accessors[primitive.indices];
  return indices.count;
}

StatusOr<std::vector<uint32_t>> GltfDecoder::DecodePrimitiveIndices(
    const tinygltf::Primitive& primitive) const {
  std::vector<uint32_t> indices_data;
  if (primitive.indices < 0) {
    // Primitive has implicit indices [0, 1, 2, 3, ...]. Create indices based on
    // entry count of a primitive attribute.
    DRACO_ASSIGN_OR_RETURN(const int num_vertices,
                           DecodePrimitiveAttributeCount(primitive));
    indices_data.reserve(num_vertices);
    for (int i = 0; i < num_vertices; i++) {
      indices_data.push_back(i);
    }
  } else {
    // Get indices from the primitive's indices property.
    const tinygltf::Accessor& indices =
        gltf_model_.accessors[primitive.indices];
    if (indices.count <= 0) {
      return Status(Status::DRACO_ERROR, "Could not convert indices.");
    }
    DRACO_ASSIGN_OR_RETURN(indices_data,
                           CopyDataAsUint32(gltf_model_, indices));
  }
  return indices_data;
}

Status GltfDecoder::DecodePrimitive(const tinygltf::Primitive& primitive,
                                    const Eigen::Matrix4d& transform_matrix) {
  if (primitive.mode != TINYGLTF_MODE_TRIANGLES &&
      primitive.mode != TINYGLTF_MODE_POINTS) {
    return Status(Status::DRACO_ERROR,
                  "Primitive does not contain triangles or points.");
  }

  // Store the transformation scale of this primitive loading as draco::Mesh.
  if (scene_ == nullptr) {
    // TODO(vytyaz): Do something for non-uniform scaling.
    const float scale = transform_matrix.col(0).norm();
    gltf_primitive_material_to_scales_[primitive.material].push_back(scale);
  }

  // Handle indices first.
  DRACO_ASSIGN_OR_RETURN(const std::vector<uint32_t> indices_data,
                         DecodePrimitiveIndices(primitive));
  const int number_of_faces = indices_data.size() / 3;
  const int number_of_points = indices_data.size();

  for (const auto& attribute : primitive.attributes) {
    const tinygltf::Accessor& accessor =
        gltf_model_.accessors[attribute.second];

    const int att_id =
        attribute_name_to_draco_mesh_attribute_id_[attribute.first];
    if (att_id == -1) {
      continue;
    }

    if (primitive.mode == TINYGLTF_MODE_TRIANGLES) {
      DRACO_RETURN_IF_ERROR(AddAttributeValuesToBuilder(
          attribute.first, accessor, indices_data, att_id, number_of_faces,
          transform_matrix, false, &mb_));
    } else {
      DRACO_RETURN_IF_ERROR(AddAttributeValuesToBuilder(
          attribute.first, accessor, indices_data, att_id, number_of_points,
          transform_matrix, false, &pb_));
    }
  }

  // Add the material data only if the material attribute is present.
  if (material_att_id_ != -1) {
    const int material_index = primitive.material;
    const auto it =
        gltf_primitive_material_to_draco_material_.find(material_index);
    if (it != gltf_primitive_material_to_draco_material_.end()) {
      if (primitive.mode == TINYGLTF_MODE_TRIANGLES) {
        DRACO_RETURN_IF_ERROR(
            AddMaterialDataToBuilder(it->second, number_of_faces, &mb_));
      } else {
        DRACO_RETURN_IF_ERROR(
            AddMaterialDataToBuilder(it->second, number_of_points, &pb_));
      }
    }
  }

  next_face_id_ += number_of_faces;
  next_point_id_ += number_of_points;
  return OkStatus();
}

Status GltfDecoder::NodeGatherAttributeAndMaterialStats(
    const tinygltf::Node& node) {
  if (node.mesh >= 0) {
    const tinygltf::Mesh& mesh = gltf_model_.meshes[node.mesh];
    for (const auto& primitive : mesh.primitives) {
      DRACO_RETURN_IF_ERROR(AccumulatePrimitiveStats(primitive));

      const auto it =
          gltf_primitive_material_to_draco_material_.find(primitive.material);
      if (it == gltf_primitive_material_to_draco_material_.end()) {
        if (preserve_material_indices_) {
          gltf_primitive_material_to_draco_material_[primitive.material] =
              primitive.material;
        } else {
          gltf_primitive_material_to_draco_material_[primitive.material] =
              gltf_primitive_material_to_draco_material_.size();
        }
      }
    }
  }
  for (int i = 0; i < node.children.size(); ++i) {
    const tinygltf::Node& child = gltf_model_.nodes[node.children[i]];
    DRACO_RETURN_IF_ERROR(NodeGatherAttributeAndMaterialStats(child));
  }

  return OkStatus();
}

Status GltfDecoder::GatherAttributeAndMaterialStats() {
  for (const auto& scene : gltf_model_.scenes) {
    for (int i = 0; i < scene.nodes.size(); ++i) {
      const tinygltf::Node& node = gltf_model_.nodes[scene.nodes[i]];
      DRACO_RETURN_IF_ERROR(NodeGatherAttributeAndMaterialStats(node));
    }
  }
  return OkStatus();
}

void GltfDecoder::SumAttributeStats(const std::string& attribute_name,
                                    int count) {
  // We know that there must be a valid entry for |attribute_name| at this time.
  mesh_attribute_data_[attribute_name].total_attribute_counts += count;
}

Status GltfDecoder::CheckTypes(const std::string& attribute_name,
                               int component_type, int type, bool normalized) {
  auto it_mad = mesh_attribute_data_.find(attribute_name);

  if (it_mad == mesh_attribute_data_.end()) {
    MeshAttributeData mad;
    mad.component_type = component_type;
    mad.attribute_type = type;
    mad.normalized = normalized;
    mesh_attribute_data_[attribute_name] = mad;
    return OkStatus();
  }
  if (it_mad->second.component_type != component_type) {
    return Status(
        Status::DRACO_ERROR,
        attribute_name + " attribute component type does not match previous.");
  }
  if (it_mad->second.attribute_type != type) {
    return Status(Status::DRACO_ERROR,
                  attribute_name + " attribute type does not match previous.");
  }
  if (it_mad->second.normalized != normalized) {
    return Status(
        Status::DRACO_ERROR,
        attribute_name +
            " attribute normalized property does not match previous.");
  }

  return OkStatus();
}

Status GltfDecoder::AccumulatePrimitiveStats(
    const tinygltf::Primitive& primitive) {
  DRACO_ASSIGN_OR_RETURN(const int indices_count,
                         DecodePrimitiveIndicesCount(primitive));
  if (primitive.mode == TINYGLTF_MODE_TRIANGLES) {
    total_face_indices_count_ += indices_count;
  } else if (primitive.mode == TINYGLTF_MODE_POINTS) {
    total_point_indices_count_ += indices_count;
  } else {
    return ErrorStatus("Unsupported primitive indices mode.");
  }

  for (const auto& attribute : primitive.attributes) {
    if (attribute.second >= gltf_model_.accessors.size()) {
      return ErrorStatus("Invalid accessor.");
    }
    const tinygltf::Accessor& accessor =
        gltf_model_.accessors[attribute.second];

    DRACO_RETURN_IF_ERROR(CheckTypes(attribute.first, accessor.componentType,
                                     accessor.type, accessor.normalized));
    SumAttributeStats(attribute.first, accessor.count);
  }
  return OkStatus();
}

template <typename BuilderT>
Status GltfDecoder::AddAttributesToDracoMesh(BuilderT* builder) {
  for (const auto& attribute : mesh_attribute_data_) {
    const GeometryAttribute::Type draco_att_type =
        GltfAttributeToDracoAttribute(attribute.first);
    if (draco_att_type == GeometryAttribute::INVALID) {
      // Map an invalid attribute to attribute id -1 that will be ignored and
      // not included in the Draco mesh.
      attribute_name_to_draco_mesh_attribute_id_[attribute.first] = -1;
      continue;
    }
    // TODO(vytyaz): Check that when glTF is decoded into a single draco::Mesh
    // the feature ID vertex attributes are consistent with geometry, e.g., the
    // number of values in attribute with name _FEATURE_ID_5 accumulated from
    // all primitives must be equal to the accumulated number of mesh vertices.
    // Furthermore, accumulated attributes should probably come from mesh
    // feature ID sets with the same labels.
    DRACO_ASSIGN_OR_RETURN(
        const int att_id,
        AddAttribute(draco_att_type, attribute.second.component_type,
                     attribute.second.attribute_type,
                     attribute.second.normalized, builder));
    attribute_name_to_draco_mesh_attribute_id_[attribute.first] = att_id;

    if (draco_att_type == draco::GeometryAttribute::GENERIC) {
      AddAppSpecificAttrName(att_id, attribute.first, builder);
    }
  }

  // Add the material attribute.
  bool add_material_attribute =
      (gltf_primitive_material_to_draco_material_.size() > 1);
  if (preserve_material_indices_ &&
      gltf_primitive_material_to_draco_material_.size() == 1 &&
      gltf_primitive_material_to_draco_material_.count(0) == 0) {
    // We have a single material, but it is not the first material of the glTF.
    // Because we need to preserve material indices, we still need to add the
    // material attribute.
    add_material_attribute = true;
  }

  if (add_material_attribute) {
    draco::DataType component_type = DT_UINT32;
    if (gltf_model_.materials.size() < 256) {
      component_type = DT_UINT8;
    } else if (gltf_model_.materials.size() < (1 << 16)) {
      component_type = DT_UINT16;
    }
    material_att_id_ =
        builder->AddAttribute(GeometryAttribute::MATERIAL, 1, component_type);
  }

  return OkStatus();
}

// Returns the index from a feature ID vertex attribute name like _FEATURE_ID_5.
int GetIndexFromFeatureIdAttributeName(const std::string& name) {
  const std::string prefix = "_FEATURE_ID_";
  const std::string number = name.substr(prefix.length());
  return std::stoi(number);
}

template <typename BuilderT>
Status GltfDecoder::AddAttributeValuesToBuilder(
    const std::string& attribute_name, const tinygltf::Accessor& accessor,
    const std::vector<uint32_t>& indices_data, int att_id,
    int number_of_elements, const Eigen::Matrix4d& transform_matrix,
    bool is_morph_target, BuilderT* builder) {
  const bool reverse_winding = Determinant(transform_matrix) < 0;
  if (attribute_name.rfind("TEXCOORD_", 0) == 0) {
    DRACO_RETURN_IF_ERROR(
        AddTexCoordToBuilder(accessor, indices_data, att_id, number_of_elements,
                             reverse_winding, is_morph_target, builder));
  } else if (attribute_name == "TANGENT") {
    const Eigen::Matrix4d matrix = UpdateMatrixForNormals(transform_matrix);
    DRACO_RETURN_IF_ERROR(
        AddTangentToBuilder(accessor, indices_data, att_id, number_of_elements,
                            matrix, reverse_winding, is_morph_target, builder));
  } else if (attribute_name == "POSITION" || attribute_name == "NORMAL") {
    const Eigen::Matrix4d matrix =
        (attribute_name == "NORMAL") ? UpdateMatrixForNormals(transform_matrix)
                                     : transform_matrix;
    const bool normalize = (attribute_name == "NORMAL" && !is_morph_target);
    DRACO_RETURN_IF_ERROR(AddTransformedDataToBuilder(
        accessor, indices_data, att_id, number_of_elements, matrix, normalize,
        reverse_winding, builder));
  } else if (attribute_name.rfind("_FEATURE_ID_") == 0) {
    DRACO_RETURN_IF_ERROR(AddFeatureIdToBuilder(
        accessor, indices_data, att_id, number_of_elements, reverse_winding,
        attribute_name, builder));
    // Populate map from the index in attribute name like _FEATURE_ID_5 to the
    // attribute index in the builder.
    const int index = GetIndexFromFeatureIdAttributeName(attribute_name);
    feature_id_attribute_indices_[index] = att_id;
  } else if (attribute_name.rfind('_', 0) == 0) {
    // This is a structural metadata property attribute with a name like
    // _DIRECTION that begins with an underscore.
    DRACO_RETURN_IF_ERROR(AddPropertyAttributeToBuilder(
        accessor, indices_data, att_id, number_of_elements, reverse_winding,
        attribute_name, builder));
  } else {
    DRACO_RETURN_IF_ERROR(AddAttributeDataByTypes(accessor, indices_data,
                                                  att_id, number_of_elements,
                                                  reverse_winding, builder));
  }
  return OkStatus();
}

template <typename BuilderT>
Status GltfDecoder::AddTangentToBuilder(
    const tinygltf::Accessor& accessor,
    const std::vector<uint32_t>& indices_data, int att_id,
    int number_of_elements, const Eigen::Matrix4d& transform_matrix,
    bool reverse_winding, bool is_morph_target, BuilderT* builder) {
  std::vector<Vector4f> data;
  if (!is_morph_target) {
    DRACO_ASSIGN_OR_RETURN(
        data, TinyGltfUtils::CopyDataAsFloat<Vector4f>(gltf_model_, accessor));

    for (int v = 0; v < data.size(); ++v) {
      Eigen::Vector4d vec4(data[v][0], data[v][1], data[v][2], 1);
      vec4 = transform_matrix * vec4;

      // Normalize the data.
      Eigen::Vector3d vec3(vec4[0], vec4[1], vec4[2]);
      vec3 = vec3.normalized();
      for (int i = 0; i < 3; ++i) {
        vec4[i] = vec3[i];
      }

      // Add back the original w component.
      vec4[3] = data[v][3];
      for (int i = 0; i < 4; ++i) {
        data[v][i] = vec4[i];
      }
    }
  } else {
    // For morph targets, load the offsets as vec3 with no normalization.
    std::vector<Vector3f> data_v3;
    DRACO_ASSIGN_OR_RETURN(data_v3, TinyGltfUtils::CopyDataAsFloat<Vector3f>(
                                        gltf_model_, accessor));
    // Copy over the data to the Vector4f vector to assign them to the builder.
    data.resize(data_v3.size());
    for (int i = 0; i < data_v3.size(); ++i) {
      data[i] = Vector4f(data_v3[i][0], data_v3[i][1], data_v3[i][2], 0.0f);
    }
  }

  DRACO_RETURN_IF_ERROR(
      SetValuesForBuilder<Vector4f>(indices_data, att_id, number_of_elements,
                                    data, reverse_winding, builder));
  return OkStatus();
}

// Helper templates for flipping texture coordinates.
template <typename T>
struct TexCoordFlipper;

// Specialization for float UVs.
template <>
struct TexCoordFlipper<Vector2f> {
  static void Flip(std::vector<Vector2f>& data) {
    for (auto& uv : data) {
      uv[1] = 1.0f - uv[1];
    }
  }
};

// Partial specialization for quantized integer UVs.
template <typename ScalarT>
struct TexCoordFlipper<VectorD<ScalarT, 2>> {
  static void Flip(std::vector<VectorD<ScalarT, 2>>& data) {
    const ScalarT max_val = std::numeric_limits<ScalarT>::max();
    for (auto& uv : data) {
      uv[1] = max_val - uv[1];
    }
  }
};

template <typename T>
void FlipTexCoord(std::vector<T>& data) {
  TexCoordFlipper<T>::Flip(data);
}

template <typename BuilderT>
Status GltfDecoder::AddTexCoordToBuilder(
    const tinygltf::Accessor& accessor,
    const std::vector<uint32_t>& indices_data, int att_id,
    int number_of_elements, bool reverse_winding, bool is_morph_target,
    BuilderT* builder) {
  // Special handling for morph targets where we support only float data.
  if (is_morph_target) {
    if (accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT) {
      return Status(Status::DRACO_ERROR,
                    "Quantized morph targets for TEXCOORD not supported.");
    }
    DRACO_ASSIGN_OR_RETURN(std::vector<Vector2f> data,
                           CopyDataAs<Vector2f>(gltf_model_, accessor));
    for (auto& uv : data) {
      uv[1] = -uv[1];
    }
    DRACO_RETURN_IF_ERROR(
        SetValuesForBuilder<Vector2f>(indices_data, att_id, number_of_elements,
                                      data, reverse_winding, builder));
    return OkStatus();
  }

  switch (accessor.componentType) {
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
      return AddTexCoordToBuilderTyped<BuilderT, VectorD<uint8_t, 2>>(
          accessor, indices_data, att_id, number_of_elements, reverse_winding,
          builder);
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
      return AddTexCoordToBuilderTyped<BuilderT, VectorD<uint16_t, 2>>(
          accessor, indices_data, att_id, number_of_elements, reverse_winding,
          builder);
    case TINYGLTF_COMPONENT_TYPE_FLOAT:
      return AddTexCoordToBuilderTyped<BuilderT, Vector2f>(
          accessor, indices_data, att_id, number_of_elements, reverse_winding,
          builder);
    default:
      return Status(Status::DRACO_ERROR,
                    "Unsupported component type for texture coordinate.");
  }
}

template <typename BuilderT, typename T>
Status GltfDecoder::AddTexCoordToBuilderTyped(
    const tinygltf::Accessor& accessor,
    const std::vector<uint32_t>& indices_data, int att_id,
    int number_of_elements, bool reverse_winding, BuilderT* builder) {
  DRACO_ASSIGN_OR_RETURN(std::vector<T> data,
                         CopyDataAs<T>(gltf_model_, accessor));
  FlipTexCoord(data);
  DRACO_RETURN_IF_ERROR(SetValuesForBuilder<T>(indices_data, att_id,
                                               number_of_elements, data,
                                               reverse_winding, builder));
  return OkStatus();
}

template <typename BuilderT>
Status GltfDecoder::AddFeatureIdToBuilder(
    const tinygltf::Accessor& accessor,
    const std::vector<uint32_t>& indices_data, int att_id,
    int number_of_elements, bool reverse_winding,
    const std::string& attribute_name, BuilderT* builder) {
  // Check that the feature ID attribute has correct type.
  const int num_components =
      TinyGltfUtils::GetNumComponentsForType(accessor.type);
  if (num_components != 1) {
    return ErrorStatus("Invalid feature ID attribute type.");
  }
  const draco::DataType draco_component_type =
      GltfComponentTypeToDracoType(accessor.componentType);
  if (draco_component_type != DT_UINT8 && draco_component_type != DT_UINT16 &&
      draco_component_type != DT_FLOAT32) {
    return ErrorStatus("Invalid feature ID attribute component type.");
  }

  // Set feature ID attribute values to mesh faces.
  DRACO_RETURN_IF_ERROR(AddAttributeDataByTypes(accessor, indices_data, att_id,
                                                number_of_elements,
                                                reverse_winding, builder));
  return OkStatus();
}

template <typename BuilderT>
Status GltfDecoder::AddPropertyAttributeToBuilder(
    const tinygltf::Accessor& accessor,
    const std::vector<uint32_t>& indices_data, int att_id,
    int number_of_elements, bool reverse_winding,
    const std::string& attribute_name, BuilderT* builder) {
  // Set property attribute values to mesh.
  DRACO_RETURN_IF_ERROR(AddAttributeDataByTypes(accessor, indices_data, att_id,
                                                number_of_elements,
                                                reverse_winding, builder));

  // Store property attribute name like _DIRECTION in Draco attribute.
  builder->SetAttributeName(att_id, attribute_name);
  return OkStatus();
}

template <typename BuilderT>
Status GltfDecoder::AddTransformedDataToBuilder(
    const tinygltf::Accessor& accessor,
    const std::vector<uint32_t>& indices_data, int att_id,
    int number_of_elements, const Eigen::Matrix4d& transform_matrix,
    bool normalize, bool reverse_winding, BuilderT* builder) {
  DRACO_ASSIGN_OR_RETURN(
      std::vector<Vector3f> data,
      TinyGltfUtils::CopyDataAsFloat<Vector3f>(gltf_model_, accessor));

  for (int v = 0; v < data.size(); ++v) {
    Eigen::Vector4d vec4(data[v][0], data[v][1], data[v][2], 1);
    vec4 = transform_matrix * vec4;
    Eigen::Vector3d vec3(vec4[0], vec4[1], vec4[2]);
    if (normalize) {
      vec3 = vec3.normalized();
    }
    for (int i = 0; i < 3; ++i) {
      data[v][i] = vec3[i];
    }
  }

  DRACO_RETURN_IF_ERROR(
      SetValuesForBuilder<Vector3f>(indices_data, att_id, number_of_elements,
                                    data, reverse_winding, builder));
  return OkStatus();
}

template <typename T>
Status GltfDecoder::SetValuesForBuilder(
    const std::vector<uint32_t>& indices_data, int att_id,
    int number_of_elements, const std::vector<T>& data, bool reverse_winding,
    TriangleSoupMeshBuilder* builder) {
  DRACO_RETURN_IF_ERROR(SetValuesPerFace(indices_data, att_id,
                                         number_of_elements, data,
                                         reverse_winding, builder));
  return OkStatus();
}

template <typename T>
Status GltfDecoder::SetValuesForBuilder(
    const std::vector<uint32_t>& indices_data, int att_id,
    int number_of_elements, const std::vector<T>& data, bool reverse_winding,
    PointCloudBuilder* builder) {
  for (int i = 0; i < number_of_elements; ++i) {
    const uint32_t v_id = indices_data[i];
    if (v_id >= data.size()) {
      return Status(Status::DRACO_ERROR, "Index out of bounds");
    }
    const PointIndex pi(v_id + next_point_id_);
    builder->SetAttributeValueForPoint(att_id, pi,
                                       GetDataContentAddress(data[v_id]));
  }
  return OkStatus();
}

template <typename T>
Status GltfDecoder::SetValuesPerFace(const std::vector<uint32_t>& indices_data,
                                     int att_id, int number_of_faces,
                                     const std::vector<T>& data,
                                     bool reverse_winding,
                                     TriangleSoupMeshBuilder* mb) {
  for (int f = 0; f < number_of_faces; ++f) {
    const int base_corner = f * 3;
    const uint32_t v_id = indices_data[base_corner];
    const int next_offset = reverse_winding ? 2 : 1;
    const int prev_offset = reverse_winding ? 1 : 2;
    const uint32_t v_next_id = indices_data[base_corner + next_offset];
    const uint32_t v_prev_id = indices_data[base_corner + prev_offset];

    if (v_id >= data.size() || v_next_id >= data.size() ||
        v_prev_id >= data.size()) {
      return Status(Status::DRACO_ERROR, "Index out of bounds");
    }

    const FaceIndex face_index(f + next_face_id_);
    mb->SetAttributeValuesForFace(att_id, face_index,
                                  GetDataContentAddress(data[v_id]),
                                  GetDataContentAddress(data[v_next_id]),
                                  GetDataContentAddress(data[v_prev_id]));
  }
  return OkStatus();
}

// Get the address of data content for arithmetic types |T|.
template <typename T>
const void* GetDataContentAddressImpl(const T& data,
                                      std::true_type /* is_arithmetic */) {
  return &data;
}

// Get the address of data content for vector types |T|.
template <typename T>
const void* GetDataContentAddressImpl(const T& data,
                                      std::false_type /* is_arithmetic */) {
  return data.data();
}

template <typename T>
const void* GltfDecoder::GetDataContentAddress(const T& data) const {
  return GetDataContentAddressImpl(data, std::is_arithmetic<T>());
}

template <typename BuilderT>
Status GltfDecoder::AddAttributeDataByTypes(
    const tinygltf::Accessor& accessor,
    const std::vector<uint32_t>& indices_data, int att_id,
    int number_of_elements, bool reverse_winding, BuilderT* builder) {
  typedef VectorD<uint8_t, 2> Vector2u8i;
  typedef VectorD<uint8_t, 3> Vector3u8i;
  typedef VectorD<uint8_t, 4> Vector4u8i;
  typedef VectorD<uint16_t, 2> Vector2u16i;
  typedef VectorD<uint16_t, 3> Vector3u16i;
  typedef VectorD<uint16_t, 4> Vector4u16i;
  typedef VectorD<int8_t, 2> Vector2i8i;
  typedef VectorD<int8_t, 3> Vector3i8i;
  typedef VectorD<int8_t, 4> Vector4i8i;
  typedef VectorD<int16_t, 2> Vector2i16i;
  typedef VectorD<int16_t, 3> Vector3i16i;
  typedef VectorD<int16_t, 4> Vector4i16i;
  typedef VectorD<int32_t, 2> Vector2i32i;
  typedef VectorD<int32_t, 3> Vector3i32i;
  typedef VectorD<int32_t, 4> Vector4i32i;
  switch (accessor.type) {
    case TINYGLTF_TYPE_SCALAR:
      switch (accessor.componentType) {
        case TINYGLTF_COMPONENT_TYPE_BYTE: {
          DRACO_ASSIGN_OR_RETURN(std::vector<int8_t> data,
                                 CopyDataAs<int8_t>(gltf_model_, accessor));
          DRACO_RETURN_IF_ERROR(SetValuesForBuilder<int8_t>(
              indices_data, att_id, number_of_elements, data, reverse_winding,
              builder));
        } break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
          DRACO_ASSIGN_OR_RETURN(std::vector<uint8_t> data,
                                 CopyDataAs<uint8_t>(gltf_model_, accessor));
          DRACO_RETURN_IF_ERROR(SetValuesForBuilder<uint8_t>(
              indices_data, att_id, number_of_elements, data, reverse_winding,
              builder));
        } break;
        case TINYGLTF_COMPONENT_TYPE_SHORT: {
          DRACO_ASSIGN_OR_RETURN(std::vector<int16_t> data,
                                 CopyDataAs<int16_t>(gltf_model_, accessor));
          DRACO_RETURN_IF_ERROR(SetValuesForBuilder<int16_t>(
              indices_data, att_id, number_of_elements, data, reverse_winding,
              builder));
        } break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
          DRACO_ASSIGN_OR_RETURN(std::vector<uint16_t> data,
                                 CopyDataAs<uint16_t>(gltf_model_, accessor));
          DRACO_RETURN_IF_ERROR(SetValuesForBuilder<uint16_t>(
              indices_data, att_id, number_of_elements, data, reverse_winding,
              builder));
        } break;
        case TINYGLTF_COMPONENT_TYPE_INT: {
          DRACO_ASSIGN_OR_RETURN(std::vector<int32_t> data,
                                 CopyDataAs<int32_t>(gltf_model_, accessor));
          DRACO_RETURN_IF_ERROR(SetValuesForBuilder<int32_t>(
              indices_data, att_id, number_of_elements, data, reverse_winding,
              builder));
        } break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: {
          DRACO_ASSIGN_OR_RETURN(std::vector<uint32_t> data,
                                 CopyDataAs<uint32_t>(gltf_model_, accessor));
          DRACO_RETURN_IF_ERROR(SetValuesForBuilder<uint32_t>(
              indices_data, att_id, number_of_elements, data, reverse_winding,
              builder));
        } break;
        case TINYGLTF_COMPONENT_TYPE_FLOAT: {
          DRACO_ASSIGN_OR_RETURN(std::vector<float> data,
                                 CopyDataAs<float>(gltf_model_, accessor));
          DRACO_RETURN_IF_ERROR(SetValuesForBuilder<float>(
              indices_data, att_id, number_of_elements, data, reverse_winding,
              builder));
        } break;
        default:
          return ErrorStatus("Add attribute data, unknown component type.");
      }
      break;
    case TINYGLTF_TYPE_VEC2:
      switch (accessor.componentType) {
        case TINYGLTF_COMPONENT_TYPE_BYTE: {
          DRACO_ASSIGN_OR_RETURN(std::vector<Vector2i8i> data,
                                 CopyDataAs<Vector2i8i>(gltf_model_, accessor));
          DRACO_RETURN_IF_ERROR(SetValuesForBuilder<Vector2i8i>(
              indices_data, att_id, number_of_elements, data, reverse_winding,
              builder));
        } break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
          DRACO_ASSIGN_OR_RETURN(std::vector<Vector2u8i> data,
                                 CopyDataAs<Vector2u8i>(gltf_model_, accessor));
          DRACO_RETURN_IF_ERROR(SetValuesForBuilder<Vector2u8i>(
              indices_data, att_id, number_of_elements, data, reverse_winding,
              builder));
        } break;
        case TINYGLTF_COMPONENT_TYPE_SHORT: {
          DRACO_ASSIGN_OR_RETURN(
              std::vector<Vector2i16i> data,
              CopyDataAs<Vector2i16i>(gltf_model_, accessor));
          DRACO_RETURN_IF_ERROR(SetValuesForBuilder<Vector2i16i>(
              indices_data, att_id, number_of_elements, data, reverse_winding,
              builder));
        } break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
          DRACO_ASSIGN_OR_RETURN(
              std::vector<Vector2u16i> data,
              CopyDataAs<Vector2u16i>(gltf_model_, accessor));
          DRACO_RETURN_IF_ERROR(SetValuesForBuilder<Vector2u16i>(
              indices_data, att_id, number_of_elements, data, reverse_winding,
              builder));
        } break;
        case TINYGLTF_COMPONENT_TYPE_INT: {
          DRACO_ASSIGN_OR_RETURN(
              std::vector<Vector2i32i> data,
              CopyDataAs<Vector2i32i>(gltf_model_, accessor));
          DRACO_RETURN_IF_ERROR(SetValuesForBuilder<Vector2i32i>(
              indices_data, att_id, number_of_elements, data, reverse_winding,
              builder));
        } break;
        case TINYGLTF_COMPONENT_TYPE_FLOAT: {
          DRACO_ASSIGN_OR_RETURN(
              std::vector<Vector2f> data,
              TinyGltfUtils::CopyDataAsFloat<Vector2f>(gltf_model_, accessor));
          DRACO_RETURN_IF_ERROR(SetValuesForBuilder<Vector2f>(
              indices_data, att_id, number_of_elements, data, reverse_winding,
              builder));
        } break;
        default:
          return Status(Status::DRACO_ERROR,
                        "Add attribute data, unknown component type.");
      }
      break;
    case TINYGLTF_TYPE_VEC3:
      switch (accessor.componentType) {
        case TINYGLTF_COMPONENT_TYPE_BYTE: {
          DRACO_ASSIGN_OR_RETURN(std::vector<Vector3i8i> data,
                                 CopyDataAs<Vector3i8i>(gltf_model_, accessor));
          DRACO_RETURN_IF_ERROR(SetValuesForBuilder<Vector3i8i>(
              indices_data, att_id, number_of_elements, data, reverse_winding,
              builder));
        } break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
          DRACO_ASSIGN_OR_RETURN(std::vector<Vector3u8i> data,
                                 CopyDataAs<Vector3u8i>(gltf_model_, accessor));
          DRACO_RETURN_IF_ERROR(SetValuesForBuilder<Vector3u8i>(
              indices_data, att_id, number_of_elements, data, reverse_winding,
              builder));
        } break;
        case TINYGLTF_COMPONENT_TYPE_SHORT: {
          DRACO_ASSIGN_OR_RETURN(
              std::vector<Vector3i16i> data,
              CopyDataAs<Vector3i16i>(gltf_model_, accessor));
          DRACO_RETURN_IF_ERROR(SetValuesForBuilder<Vector3i16i>(
              indices_data, att_id, number_of_elements, data, reverse_winding,
              builder));
        } break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
          DRACO_ASSIGN_OR_RETURN(
              std::vector<Vector3u16i> data,
              CopyDataAs<Vector3u16i>(gltf_model_, accessor));
          DRACO_RETURN_IF_ERROR(SetValuesForBuilder<Vector3u16i>(
              indices_data, att_id, number_of_elements, data, reverse_winding,
              builder));
        } break;
        case TINYGLTF_COMPONENT_TYPE_INT: {
          DRACO_ASSIGN_OR_RETURN(
              std::vector<Vector3i32i> data,
              CopyDataAs<Vector3i32i>(gltf_model_, accessor));
          DRACO_RETURN_IF_ERROR(SetValuesForBuilder<Vector3i32i>(
              indices_data, att_id, number_of_elements, data, reverse_winding,
              builder));
        } break;
        case TINYGLTF_COMPONENT_TYPE_FLOAT: {
          DRACO_ASSIGN_OR_RETURN(
              std::vector<Vector3f> data,
              TinyGltfUtils::CopyDataAsFloat<Vector3f>(gltf_model_, accessor));
          DRACO_RETURN_IF_ERROR(SetValuesForBuilder<Vector3f>(
              indices_data, att_id, number_of_elements, data, reverse_winding,
              builder));
        } break;
        default:
          return Status(Status::DRACO_ERROR,
                        "Add attribute data, unknown component type.");
      }
      break;
    case TINYGLTF_TYPE_VEC4:
      switch (accessor.componentType) {
        case TINYGLTF_COMPONENT_TYPE_BYTE: {
          DRACO_ASSIGN_OR_RETURN(std::vector<Vector4i8i> data,
                                 CopyDataAs<Vector4i8i>(gltf_model_, accessor));
          DRACO_RETURN_IF_ERROR(SetValuesForBuilder<Vector4i8i>(
              indices_data, att_id, number_of_elements, data, reverse_winding,
              builder));
        } break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
          DRACO_ASSIGN_OR_RETURN(std::vector<Vector4u8i> data,
                                 CopyDataAs<Vector4u8i>(gltf_model_, accessor));
          DRACO_RETURN_IF_ERROR(SetValuesForBuilder<Vector4u8i>(
              indices_data, att_id, number_of_elements, data, reverse_winding,
              builder));
        } break;
        case TINYGLTF_COMPONENT_TYPE_SHORT: {
          DRACO_ASSIGN_OR_RETURN(
              std::vector<Vector4i16i> data,
              CopyDataAs<Vector4i16i>(gltf_model_, accessor));
          DRACO_RETURN_IF_ERROR(SetValuesForBuilder<Vector4i16i>(
              indices_data, att_id, number_of_elements, data, reverse_winding,
              builder));
        } break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
          DRACO_ASSIGN_OR_RETURN(
              std::vector<Vector4u16i> data,
              CopyDataAs<Vector4u16i>(gltf_model_, accessor));
          DRACO_RETURN_IF_ERROR(SetValuesForBuilder<Vector4u16i>(
              indices_data, att_id, number_of_elements, data, reverse_winding,
              builder));
        } break;
        case TINYGLTF_COMPONENT_TYPE_INT: {
          DRACO_ASSIGN_OR_RETURN(
              std::vector<Vector4i32i> data,
              CopyDataAs<Vector4i32i>(gltf_model_, accessor));
          DRACO_RETURN_IF_ERROR(SetValuesForBuilder<Vector4i32i>(
              indices_data, att_id, number_of_elements, data, reverse_winding,
              builder));
        } break;
        case TINYGLTF_COMPONENT_TYPE_FLOAT: {
          DRACO_ASSIGN_OR_RETURN(
              std::vector<Vector4f> data,
              TinyGltfUtils::CopyDataAsFloat<Vector4f>(gltf_model_, accessor));
          DRACO_RETURN_IF_ERROR(SetValuesForBuilder<Vector4f>(
              indices_data, att_id, number_of_elements, data, reverse_winding,
              builder));
        } break;
        default:
          return Status(Status::DRACO_ERROR,
                        "Add attribute data, unknown component type.");
      }
      break;
    default:
      return Status(Status::DRACO_ERROR, "Add attribute data, unknown type.");
  }
  return OkStatus();
}

template <typename T>
Status GltfDecoder::CopyTextures(T* owner) {
  for (int i = 0; i < gltf_model_.images.size(); ++i) {
    const tinygltf::Image& image = gltf_model_.images[i];
    if (image.width == -1 || image.height == -1 || image.component == -1) {
      // TinyGLTF does not return an error when it cannot find an image. It will
      // add an image with negative values.
      return Status(Status::DRACO_ERROR, "Error loading image.");
    }

    std::unique_ptr<Texture> draco_texture(new Texture());

    // Update mapping between glTF images and textures in the texture library.
    gltf_image_to_draco_texture_[i] = draco_texture.get();

    DRACO_ASSIGN_OR_RETURN(std::unique_ptr<SourceImage> source_image,
                           GetSourceImage(gltf_model_, image, *draco_texture));
    if (source_image->encoded_data().empty() &&
        !source_image->filename().empty()) {
      // Update filename of source image to be relative of the glTF file.
      std::string dirname;
      std::string basename;
      SplitPath(input_file_name_, &dirname, &basename);
      source_image->set_filename(dirname + "/" + source_image->filename());
    }
    draco_texture->set_source_image(*source_image);

    owner->GetMaterialLibrary().MutableTextureLibrary().PushTexture(
        std::move(draco_texture));
  }
  return OkStatus();
}

void GltfDecoder::SetAttributePropertiesOnDracoMesh(Mesh* mesh) {
  for (const auto& mad : mesh_attribute_data_) {
    const int att_id = attribute_name_to_draco_mesh_attribute_id_[mad.first];
    if (att_id == -1) {
      continue;
    }
    if (mad.second.normalized) {
      mesh->attribute(att_id)->set_normalized(true);
    }
  }
}

Status GltfDecoder::AddMaterialsToDracoMesh(Mesh* mesh) {
  bool is_normal_map_used = false;

  int default_material_index = -1;
  auto it = gltf_primitive_material_to_draco_material_.find(-1);
  if (it != gltf_primitive_material_to_draco_material_.end()) {
    default_material_index = it->second;
  }

  int output_material_index = 0;
  for (int input_material_index = 0;
       input_material_index < gltf_model_.materials.size();
       ++input_material_index) {
    it = gltf_primitive_material_to_draco_material_.find(input_material_index);
    if (it == gltf_primitive_material_to_draco_material_.end()) {
      continue;
    }
    output_material_index = it->second;
    if (default_material_index == input_material_index) {
      // Insert a default material here for primitives that did not have a
      // material index.
      mesh->GetMaterialLibrary().MutableMaterial(output_material_index);
    }

    Material* const output_material =
        mesh->GetMaterialLibrary().MutableMaterial(output_material_index);
    DRACO_RETURN_IF_ERROR(
        AddGltfMaterial(input_material_index, output_material));
    if (output_material->GetTextureMapByType(
            TextureMap::NORMAL_TANGENT_SPACE)) {
      is_normal_map_used = true;
    }
  }

  return OkStatus();
}

template <typename BuilderT>
Status GltfDecoder::AddMaterialDataToBuilder(int material_value,
                                             int number_of_elements,
                                             BuilderT* builder) {
  if (gltf_primitive_material_to_draco_material_.size() < 256) {
    const uint8_t typed_material_value = material_value;
    DRACO_RETURN_IF_ERROR(AddMaterialDataToBuilderInternal<uint8_t>(
        typed_material_value, number_of_elements, builder));
  } else if (gltf_primitive_material_to_draco_material_.size() < (1 << 16)) {
    const uint16_t typed_material_value = material_value;
    DRACO_RETURN_IF_ERROR(AddMaterialDataToBuilderInternal<uint16_t>(
        typed_material_value, number_of_elements, builder));
  } else {
    const uint32_t typed_material_value = material_value;
    DRACO_RETURN_IF_ERROR(AddMaterialDataToBuilderInternal<uint32_t>(
        typed_material_value, number_of_elements, builder));
  }
  return OkStatus();
}

template <typename T>
Status GltfDecoder::AddMaterialDataToBuilderInternal(
    T material_value, int number_of_faces, TriangleSoupMeshBuilder* builder) {
  for (int f = 0; f < number_of_faces; ++f) {
    const FaceIndex face_index(f + next_face_id_);
    builder->SetPerFaceAttributeValueForFace(material_att_id_, face_index,
                                             &material_value);
  }
  return OkStatus();
}

template <typename T>
Status GltfDecoder::AddMaterialDataToBuilderInternal(
    T material_value, int number_of_points, PointCloudBuilder* builder) {
  for (int pi = 0; pi < number_of_points; ++pi) {
    const PointIndex point_index(pi + next_point_id_);
    builder->SetAttributeValueForPoint(material_att_id_, point_index,
                                       &material_value);
  }
  return OkStatus();
}

Status GltfDecoder::CheckAndAddTextureToDracoMaterial(
    int texture_index, int tex_coord_attribute_index,
    const tinygltf::ExtensionMap& tex_info_ext, Material* material,
    TextureMap::Type type) {
  if (texture_index < 0) {
    return OkStatus();
  }

  if (texture_index >= gltf_model_.textures.size()) {
    return Status(Status::DRACO_ERROR, "Texture index out of bounds.");
  }
  const tinygltf::Texture& input_texture = gltf_model_.textures[texture_index];
  int source_index = input_texture.source;

  const auto texture_it = gltf_image_to_draco_texture_.find(source_index);
  if (texture_it == gltf_image_to_draco_texture_.end()) {
    return ErrorStatus("Missing texture image at source index " +
                       std::to_string(source_index));
  }
  Texture* const texture = texture_it->second;
  // Default GLTF 2.0 sampler uses REPEAT mode along both S and T directions.
  TextureMap::WrappingMode wrapping_mode(TextureMap::REPEAT);
  TextureMap::FilterType min_filter = TextureMap::UNSPECIFIED;
  TextureMap::FilterType mag_filter = TextureMap::UNSPECIFIED;

  if (input_texture.sampler >= 0) {
    if (input_texture.sampler >= gltf_model_.samplers.size()) {
      return Status(Status::DRACO_ERROR, "Sampler index out of bounds.");
    }
    const tinygltf::Sampler& sampler =
        gltf_model_.samplers[input_texture.sampler];
    DRACO_ASSIGN_OR_RETURN(wrapping_mode.s,
                           TinyGltfToDracoAxisWrappingMode(sampler.wrapS));
    DRACO_ASSIGN_OR_RETURN(wrapping_mode.t,
                           TinyGltfToDracoAxisWrappingMode(sampler.wrapT));
    DRACO_ASSIGN_OR_RETURN(min_filter,
                           TinyGltfToDracoFilterType(sampler.minFilter));
    DRACO_ASSIGN_OR_RETURN(mag_filter,
                           TinyGltfToDracoFilterType(sampler.magFilter));
  }
  if (tex_coord_attribute_index < 0 || tex_coord_attribute_index > 1) {
    return Status(Status::DRACO_ERROR, "Incompatible tex coord index.");
  }
  TextureTransform transform;
  DRACO_ASSIGN_OR_RETURN(const bool has_transform,
                         CheckKhrTextureTransform(tex_info_ext, &transform));
  if (has_transform) {
    DRACO_RETURN_IF_ERROR(material->SetTextureMap(
        texture, type, wrapping_mode, min_filter, mag_filter, transform,
        tex_coord_attribute_index));
  } else {
    DRACO_RETURN_IF_ERROR(material->SetTextureMap(texture, type, wrapping_mode,
                                                  min_filter, mag_filter,
                                                  tex_coord_attribute_index));
  }
  return OkStatus();
}

Status GltfDecoder::DecodeGltfToScene() {
  DRACO_RETURN_IF_ERROR(AddLightsToScene());
  DRACO_RETURN_IF_ERROR(AddMaterialsVariantsNamesToScene());
  DRACO_RETURN_IF_ERROR(CopyTextures<Scene>(scene_.get()));
  DRACO_RETURN_IF_ERROR(AddStructuralMetadataToGeometry(scene_.get()));
  for (const tinygltf::Scene& scene : gltf_model_.scenes) {
    for (int i = 0; i < scene.nodes.size(); ++i) {
      DRACO_RETURN_IF_ERROR(
          DecodeNodeForScene(scene.nodes[i], kInvalidSceneNodeIndex));
      scene_->AddRootNodeIndex(gltf_node_to_scenenode_index_[scene.nodes[i]]);
    }
  }
  DRACO_RETURN_IF_ERROR(AddTopLevelPassThroughExtensionsToScene(scene_.get()));
  DRACO_RETURN_IF_ERROR(AddAnimationsToScene());
  DRACO_RETURN_IF_ERROR(AddMaterialsToScene());
  DRACO_RETURN_IF_ERROR(AddSkinsToScene());
  MoveNonMaterialTextures(scene_.get());
  DRACO_RETURN_IF_ERROR(AddAssetMetadata(scene_.get()));

  // Cesium RTC
  const auto rtc_it = gltf_model_.extensions.find("CESIUM_RTC");
  if (rtc_it != gltf_model_.extensions.end()) {
    auto center = gltf_model_.extensions["CESIUM_RTC"].Get("center");
    std::vector<double> cesium_rtc;

    cesium_rtc.push_back(center.Get(0).GetNumberAsDouble());
    cesium_rtc.push_back(center.Get(1).GetNumberAsDouble());
    cesium_rtc.push_back(center.Get(2).GetNumberAsDouble());
    scene_->SetCesiumRtc(cesium_rtc);
  }

  return OkStatus();
}

Status GltfDecoder::AddLightsToScene() {
  // Add all lights to Draco scene.
  for (const auto& light : gltf_model_.lights) {
    // Add a new light to the scene.
    const LightIndex light_index = scene_->AddLight();
    Light* scene_light = scene_->GetLight(light_index);

    // Decode light type.
    const std::map<std::string, Light::Type> types = {
        {"directional", Light::DIRECTIONAL},
        {"point", Light::POINT},
        {"spot", Light::SPOT}};
    if (types.count(light.type) == 0) {
      return ErrorStatus("Light type is invalid.");
    }
    scene_light->SetType(types.at(light.type));

    // Decode spot light properties.
    if (scene_light->GetType() == Light::SPOT) {
      scene_light->SetInnerConeAngle(light.spot.innerConeAngle);
      scene_light->SetOuterConeAngle(light.spot.outerConeAngle);
    }

    // Decode other light properties.
    scene_light->SetName(light.name);
    if (!light.color.empty()) {  // Empty means that color is not specified.
      if (light.color.size() != 3) {
        return ErrorStatus("Light color is malformed.");
      }
      scene_light->SetColor(
          Vector3f(light.color[0], light.color[1], light.color[2]));
    }
    scene_light->SetIntensity(light.intensity);
    if (light.range != 0.0) {  // Zero means that range is not specified.
      if (light.range < 0.0) {
        return ErrorStatus("Light range must be positive.");
      }
      scene_light->SetRange(light.range);
    }
  }
  return OkStatus();
}

Status GltfDecoder::AddMaterialsVariantsNamesToScene() {
  // Check whether the scene has materials variants.
  const auto& e = gltf_model_.extensions.find("KHR_materials_variants");
  if (e == gltf_model_.extensions.end()) {
    // The scene has no materials variants.
    return OkStatus();
  }

  // Decode all materials variants names into Draco scene from JSON like this:
  //   "KHR_materials_variants": {
  //     "variants": [
  //       {"name": "Loki" },
  //       {"name": "Odin" },
  //     ]
  //   }
  const tinygltf::Value::Object& o = e->second.Get<tinygltf::Value::Object>();
  const auto& variants = o.find("variants");
  if (variants == o.end()) {
    return ErrorStatus("Materials variants extension with names is malformed.");
  }
  const tinygltf::Value& variants_array = variants->second;
  if (!variants_array.IsArray()) {
    return ErrorStatus("Materials variants names array is malformed.");
  }
  for (int i = 0; i < variants_array.Size(); i++) {
    const auto& variant_object = variants_array.Get(i);
    if (!variant_object.IsObject() || !variant_object.Has("name")) {
      return ErrorStatus("Materials variants name is missing.");
    }
    const auto& name_string = variant_object.Get("name");
    if (!name_string.IsString()) {
      return ErrorStatus("Materials variant name is malformed.");
    }
    const std::string& name = name_string.Get<std::string>();
    scene_->GetMaterialLibrary().AddMaterialsVariant(name);
  }
  return OkStatus();
}

template <typename GeometryT>
Status GltfDecoder::AddStructuralMetadataToGeometry(GeometryT* geometry) {
  // Check whether the glTF model has structural metadata.
  const auto& e = gltf_model_.extensions.find("EXT_structural_metadata");
  if (e == gltf_model_.extensions.end()) {
    // The glTF model has no structural metadata.
    return OkStatus();
  }
  const tinygltf::Value::Object& o = e->second.Get<tinygltf::Value::Object>();

  // Decode structural metadata schema.
  DRACO_RETURN_IF_ERROR(AddStructuralMetadataSchemaToGeometry(o, geometry));

  // Decode structural metadata property tables.
  DRACO_RETURN_IF_ERROR(AddPropertyTablesToGeometry(o, geometry));

  // Decode structural metadata property attributes.
  DRACO_RETURN_IF_ERROR(AddPropertyAttributesToGeometry(o, geometry));

  // Decode structural metadata property textures.
  DRACO_RETURN_IF_ERROR(AddPropertyTexturesToGeometry(o, geometry));

  // Check that structural metadata has either property tables, or property
  // attribute, or property textures (in the future).
  if (geometry->GetStructuralMetadata().NumPropertyTables() == 0 &&
      geometry->GetStructuralMetadata().NumPropertyAttributes() == 0 &&
      geometry->GetStructuralMetadata().NumPropertyTextures() == 0) {
    return ErrorStatus(
        "Structural metadata has no property tables / attributes / textures.");
  }
  return OkStatus();
}

Status GltfDecoder::AddTopLevelPassThroughExtensionsToScene(Scene* scene) {
  // Top-level glTF extensions pass-through allowlist.
  const std::vector<std::string> allow_list_pass_through_extensions = {
      "KHR_interactivity"};

  // Copy JSON contents of allowed extensions to the |scene|.
  auto& pass_through_extensions = scene->GetPassThroughExtensions();
  for (const auto& extension_name : allow_list_pass_through_extensions) {
    const auto& extension = gltf_model_.extensions.find(extension_name);
    if (extension != gltf_model_.extensions.end()) {
      // Create a new JSON entity named after the extension name.
      JsonEntity& json_extension = pass_through_extensions[extension_name];
      json_extension = JsonEntity(extension_name);
      DRACO_RETURN_IF_ERROR(ConvertValue(extension->second, json_extension));

      // Convert glTF node indices in KHR_interactivity to Draco scene node
      // indices.
      if (extension_name == "KHR_interactivity") {
        DRACO_RETURN_IF_ERROR(FinalizeInteractivityExtension(json_extension));
      }
    }
  }
  return OkStatus();
}

Status GltfDecoder::ConvertValue(const tinygltf::Value& value,
                                 JsonEntity& entity) {
  // Output the |value| according to its type.
  if (value.IsBool()) {
    entity.SetBoolean(value.Get<bool>());
  } else if (value.IsInt()) {
    entity.SetInteger(value.Get<int>());
  } else if (value.IsReal()) {
    entity.SetDouble(value.Get<double>());
  } else if (value.IsString()) {
    entity.SetString(value.Get<std::string>());
  } else if (value.IsBinary()) {
    return ErrorStatus("Binary value is not supported.");
  } else if (value.IsArray()) {
    auto& items = entity.SetArray();
    for (int i = 0; i < value.ArrayLen(); i++) {
      items.emplace_back();
      DRACO_RETURN_IF_ERROR(ConvertValue(value.Get(i), items.back()));
    }
  } else if (value.IsObject()) {
    auto& fields = entity.SetObject();
    for (const auto& name : value.Keys()) {
      fields.emplace_back(name);
      DRACO_RETURN_IF_ERROR(ConvertValue(value.Get(name), fields.back()));
    }
  }
  return OkStatus();
}

// Splits |str| into tokens at |delimiter|.
std::vector<std::string> Tokenize(const std::string& str, char delimiter) {
  std::vector<std::string> tokens;
  std::string s = str;
  size_t pos = 0;
  while ((pos = s.find(delimiter)) != std::string::npos) {
    tokens.push_back(s.substr(0, pos));
    s.erase(0, pos + 1);
  }
  tokens.push_back(s);
  return tokens;
}

// Merges |tokens| into a string using a |delimiter|.
std::string Merge(const std::vector<std::string>& tokens, char delimiter) {
  std::stringstream ss;
  for (int i = 0; i < tokens.size() - 1; i++) {
    ss << tokens[i] << delimiter;
  }
  if (!tokens.empty()) {
    ss << tokens.back();
  }
  return ss.str();
}

Status GltfDecoder::FinalizeInteractivityExtension(JsonEntity& extension) {
  // KHR_interactivity extension must be a JSON object.
  if (extension.GetType() != JsonEntity::OBJECT) {
    return ErrorStatus("KHR_interactivity extension is not an object.");
  }

  // Nothing to be done if extension has no "nodes" field.
  JsonEntity* const nodes = extension.GetObjectEntityByName("nodes");
  if (nodes == nullptr) {
    return OkStatus();
  }

  // The "nodes" field must be a JSON array.
  if (nodes->GetType() != JsonEntity::ARRAY) {
    return ErrorStatus("KHR_interactivity extension nodes is not an array.");
  }

  // Iterate over all "nodes" entries.
  for (JsonEntity& node : nodes->GetArray()) {
    // The "nodes" entries must be JSON objects.
    if (node.GetType() != JsonEntity::OBJECT) {
      return ErrorStatus("KHR_interactivity extension node is not an object.");
    }

    // Nothing to be done for this entry if it has no "configuration" field.
    JsonEntity* const configuration =
        node.GetObjectEntityByName("configuration");
    if (configuration == nullptr) {
      continue;
    }

    // The "configuration" field must be a JSON object.
    if (configuration->GetType() != JsonEntity::OBJECT) {
      return ErrorStatus(
          "KHR_interactivity extension configuration is not an object.");
    }

    // Iterate over all fields in "configuration".
    for (JsonEntity& item : configuration->GetObject()) {
      // Both "nodeIndex" and "pointer"/"path" fields have a "value" array.
      JsonEntity* const value = item.GetObjectEntityByName("value");
      if (value == nullptr || value->GetType() != JsonEntity::ARRAY) {
        continue;
      }

      for (JsonEntity& element : value->GetArray()) {
        if (item.GetName() == "nodeIndex") {
          // Convert node index stored as an integer.
          if (element.GetType() == JsonEntity::INTEGER) {
            const int gltf_node_index = element.GetInteger();
            const auto it = gltf_node_to_scenenode_index_.find(gltf_node_index);
            if (it != gltf_node_to_scenenode_index_.end()) {
              element.SetInteger(it->second.value());
            }
          }
        } else if (item.GetName() == "pointer" || item.GetName() == "path") {
          // Convert "node index" in "/nodes/<index>/..." or "nodes/<index>/..."
          // containing node indices as in path strings.
          if (element.GetType() == JsonEntity::STRING) {
            std::vector<std::string> tokens =
                Tokenize(element.GetString(), '/');
            if (tokens.size() < 2) {
              continue;
            }
            const int index = tokens[0].empty() ? 1 : 0;
            if (tokens.size() < index + 2 || tokens[index] != "nodes") {
              continue;
            }
            // Note that std::atoi() returns 0 for non-numerical input.
            const int gltf_node_index =
                std::atoi(tokens[index + 1].c_str());  // NOLINT
            const auto it = gltf_node_to_scenenode_index_.find(gltf_node_index);
            if (it == gltf_node_to_scenenode_index_.end()) {
              return Status(Status::DRACO_ERROR,
                            "KHR_interactivity path not found.");
            }
            const int draco_node_index = it->second.value();
            tokens[index + 1] = std::to_string(draco_node_index);
            element.GetString() = Merge(tokens, '/');
          }
        }
      }
    }
  }
  return OkStatus();
}

template <typename GeometryT>
Status GltfDecoder::AddStructuralMetadataSchemaToGeometry(
    const tinygltf::Value::Object& extension, GeometryT* geometry) {
  const auto& value = extension.find("schema");
  if (value == extension.end()) {
    return ErrorStatus("Structural metadata extension has no schema.");
  }
  const tinygltf::Value& object = value->second;
  if (!object.IsObject()) {
    return ErrorStatus("Structural metadata extension schema is malformed.");
  }

  // Decodes tinygltf::Value into StructuralMetadataSchema::Object.
  struct SchemaParser {
    static Status Parse(const tinygltf::Value& value,
                        StructuralMetadataSchema::Object* object) {
      switch (value.Type()) {
        case tinygltf::OBJECT_TYPE: {
          for (auto& it : value.Get<tinygltf::Value::Object>()) {
            object->SetObjects().emplace_back(it.first);
            DRACO_RETURN_IF_ERROR(
                Parse(it.second, &object->SetObjects().back()));
          }
        } break;
        case tinygltf::ARRAY_TYPE: {
          for (int i = 0; i < value.ArrayLen(); ++i) {
            object->SetArray().emplace_back();
            DRACO_RETURN_IF_ERROR(
                Parse(value.Get(i), &object->SetArray().back()));
          }
        } break;
        case tinygltf::STRING_TYPE:
          object->SetString(value.Get<std::string>());
          break;
        case tinygltf::INT_TYPE:
          object->SetInteger(value.Get<int>());
          break;
        case tinygltf::BOOL_TYPE:
          object->SetBoolean(value.Get<bool>());
          break;
        case tinygltf::REAL_TYPE:
        case tinygltf::BINARY_TYPE:
        case tinygltf::NULL_TYPE:
        default:
          // Not used in the schema JSON.
          return ErrorStatus("Unsupported JSON type in schema.");
      }
      return OkStatus();
    }
  };

  // Parse schema of the structural metadata and set it to |geometry|.
  StructuralMetadataSchema schema;
  DRACO_RETURN_IF_ERROR(SchemaParser::Parse(object, &schema.json));
  DRACO_RETURN_IF_ERROR(CheckStructuralMetadataSchema(schema));
  geometry->GetStructuralMetadata().SetSchema(schema);
  return OkStatus();
}

Status GltfDecoder::CheckStructuralMetadataSchema(
    const StructuralMetadataSchema& schema) {
  // Check that enum value type is "UINT16", if provided.
  // TODO(vytyaz): Support other enum value types.
  const StructuralMetadataSchema::Object& json = schema.json;
  const auto* const enums_object = json.GetObjectByName("enums");
  if (enums_object == nullptr) {
    return OkStatus();
  }
  for (const auto& enum_object : enums_object->GetObjects()) {
    const auto* const value_type = enum_object.GetObjectByName("valueType");
    if (value_type == nullptr) {
      // The value type is missing, it is "UINT16" by default, so we are Ok.
      continue;
    }
    if (value_type->GetType() != StructuralMetadataSchema::Object::STRING) {
      return ErrorStatus(
          "EXT_structural_metadata schema enum valueType must be a string.");
    }
    if (value_type->GetString() != "UINT16") {
      return ErrorStatus(
          "EXT_structural_metadata schema enum valueType must be UINT16 "
          "(provided value " +
          value_type->GetString() + " is not supported).");
    }
  }
  return OkStatus();
}

template <typename GeometryT>
Status GltfDecoder::AddPropertyTablesToGeometry(
    const tinygltf::Value::Object& extension, GeometryT* geometry) {
  const auto& tables = extension.find("propertyTables");
  if (tables == extension.end()) {
    // Structural metadata has no property tables.
    return OkStatus();
  }
  const tinygltf::Value& tables_array = tables->second;
  if (!tables_array.IsArray()) {
    return ErrorStatus("Property tables array is malformed.");
  }

  // Loop over all property tables.
  for (int i = 0; i < tables_array.Size(); i++) {
    // Create a property table and populate it below.
    std::unique_ptr<PropertyTable> property_table(new PropertyTable());

    const auto& object = tables_array.Get(i);
    if (!object.IsObject()) {
      return ErrorStatus("Property table is malformed.");
    }
    const auto o = object.Get<tinygltf::Value::Object>();

    // The "class" property is required.
    bool success;
    std::string str_value;
    DRACO_ASSIGN_OR_RETURN(success, DecodeString("class", o, &str_value));
    if (success) {
      property_table->SetClass(str_value);
    } else {
      return ErrorStatus("Property table class is malformed.");
    }

    // The "count" property is required.
    int int_value;
    DRACO_ASSIGN_OR_RETURN(success, DecodeInt("count", o, &int_value));
    if (success) {
      property_table->SetCount(int_value);
    } else {
      return ErrorStatus("Property count is malformed.");
    }

    // The "name" property is optional.
    DRACO_ASSIGN_OR_RETURN(success, DecodeString("name", o, &str_value));
    if (success) {
      property_table->SetName(str_value);
    }

    // Decode property table properties (columns).
    {
      constexpr char kName[] = "properties";
      if (!object.Has(kName)) {
        return ErrorStatus("Property table is malformed.");
      }
      const tinygltf::Value& value = object.Get(kName);
      if (!value.IsObject()) {
        return ErrorStatus("Property table properties property is malformed.");
      }

      // Loop over property table properties.
      for (const auto& key : value.Keys()) {
        // Create a property table property and populate it below.
        std::unique_ptr<PropertyTable::Property> property(
            new PropertyTable::Property());

        const auto& property_object = value.Get(key);
        if (!property_object.IsObject()) {
          return ErrorStatus("Property entry is malformed.");
        }
        property->SetName(key);
        const auto o = property_object.Get<tinygltf::Value::Object>();

        // The "values" property is required.
        DRACO_ASSIGN_OR_RETURN(success, DecodePropertyTableData(
                                            "values", o, &property->GetData()));
        if (!success) {
          return ErrorStatus("Property values property is malformed.");
        }

        // All other properties are not required.
        DRACO_ASSIGN_OR_RETURN(success,
                               DecodeString("stringOffsetType", o, &str_value));
        if (success) {
          property->GetStringOffsets().type = str_value;
        }
        DRACO_ASSIGN_OR_RETURN(success,
                               DecodeString("arrayOffsetType", o, &str_value));
        if (success) {
          property->GetArrayOffsets().type = str_value;
        }
        DRACO_ASSIGN_OR_RETURN(
            success, DecodePropertyTableData(
                         "arrayOffsets", o, &property->GetArrayOffsets().data));
        DRACO_ASSIGN_OR_RETURN(
            success,
            DecodePropertyTableData("stringOffsets", o,
                                    &property->GetStringOffsets().data));

        // Add property to the property table.
        property_table->AddProperty(std::move(property));
      }
    }

    // Add property table to structural metadata.
    geometry->GetStructuralMetadata().AddPropertyTable(
        std::move(property_table));
  }
  return OkStatus();
}

template <typename GeometryT>
Status GltfDecoder::AddPropertyAttributesToGeometry(
    const tinygltf::Value::Object& extension, GeometryT* geometry) {
  const auto& attributes = extension.find("propertyAttributes");
  if (attributes == extension.end()) {
    // Structural metadata has no property attributes.
    return OkStatus();
  }
  const tinygltf::Value& attributes_array = attributes->second;
  if (!attributes_array.IsArray()) {
    return ErrorStatus("Property attributes array is malformed.");
  }

  // Loop over all property attributes.
  for (int i = 0; i < attributes_array.Size(); i++) {
    // Create a property attribute and populate it below.
    std::unique_ptr<PropertyAttribute> property_attribute(
        new PropertyAttribute());

    const auto& object = attributes_array.Get(i);
    if (!object.IsObject()) {
      return ErrorStatus("Property attribute is malformed.");
    }
    const auto o = object.Get<tinygltf::Value::Object>();

    // The "class" property is required.
    bool success;
    std::string str_value;
    DRACO_ASSIGN_OR_RETURN(success, DecodeString("class", o, &str_value));
    if (success) {
      property_attribute->SetClass(str_value);
    } else {
      return ErrorStatus("Property attribute class is malformed.");
    }

    // The "name" property is optional.
    DRACO_ASSIGN_OR_RETURN(success, DecodeString("name", o, &str_value));
    if (success) {
      property_attribute->SetName(str_value);
    }

    // Decode property attribute properties.
    {
      constexpr char kName[] = "properties";
      if (!object.Has(kName)) {
        return ErrorStatus("Property attribute is malformed.");
      }
      const tinygltf::Value& value = object.Get(kName);
      if (!value.IsObject()) {
        return ErrorStatus(
            "Property attribute properties property is malformed.");
      }

      // Loop over property attribute properties.
      for (const auto& key : value.Keys()) {
        // Create a property attribute property and populate it below.
        std::unique_ptr<PropertyAttribute::Property> property(
            new PropertyAttribute::Property());

        // Decode property name corresponding to a schema class property name.
        const auto& property_object = value.Get(key);
        if (!property_object.IsObject()) {
          return ErrorStatus("Property entry is malformed.");
        }
        property->SetName(key);
        const auto o = property_object.Get<tinygltf::Value::Object>();

        // The "attribute" property is required.
        DRACO_ASSIGN_OR_RETURN(success,
                               DecodeString("attribute", o, &str_value));
        if (success) {
          property->SetAttributeName(str_value);
        } else {
          return ErrorStatus("Property attribute is malformed.");
        }

        // Add property to the property attribute.
        property_attribute->AddProperty(std::move(property));
      }
    }

    // Add property attribute to structural metadata.
    geometry->GetStructuralMetadata().AddPropertyAttribute(
        std::move(property_attribute));
  }
  return OkStatus();
}

template <typename GeometryT>
Status GltfDecoder::AddPropertyTexturesToGeometry(
    const tinygltf::Value::Object& extension, GeometryT* geometry) {
  const auto& textures = extension.find("propertyTextures");
  if (textures == extension.end()) {
    // Structural metadata has no property textures.
    return OkStatus();
  }
  const tinygltf::Value& textures_array = textures->second;
  if (!textures_array.IsArray()) {
    return ErrorStatus("Property textures array is malformed.");
  }

  // Loop over all property textures.
  for (int i = 0; i < textures_array.Size(); i++) {
    // Create a property texture and populate it below.
    std::unique_ptr<PropertyTexture> property_texture(new PropertyTexture());

    const auto& object = textures_array.Get(i);
    if (!object.IsObject()) {
      return ErrorStatus("Property texture is malformed.");
    }
    const auto o = object.Get<tinygltf::Value::Object>();

    // The "class" property is required.
    bool success;
    std::string str_value;
    DRACO_ASSIGN_OR_RETURN(success, DecodeString("class", o, &str_value));
    if (success) {
      property_texture->SetClass(str_value);
    } else {
      return ErrorStatus("Property texture class is malformed.");
    }

    // The "name" property is optional.
    DRACO_ASSIGN_OR_RETURN(success, DecodeString("name", o, &str_value));
    if (success) {
      property_texture->SetName(str_value);
    }

    // Decode property texture properties.
    {
      constexpr char kName[] = "properties";
      if (!object.Has(kName)) {
        return ErrorStatus("Property texture is malformed.");
      }
      const tinygltf::Value& value = object.Get(kName);
      if (!value.IsObject()) {
        return ErrorStatus(
            "Property texture properties property is malformed.");
      }

      // Loop over property texture properties.
      for (const auto& key : value.Keys()) {
        // Create a property texture property and populate it below.
        std::unique_ptr<PropertyTexture::Property> property(
            new PropertyTexture::Property());

        // Decode property name corresponding to a schema class property name.
        const auto& property_object = value.Get(key);
        if (!property_object.IsObject()) {
          return ErrorStatus("Property entry is malformed.");
        }
        property->SetName(key);
        const auto o = property_object.Get<tinygltf::Value::Object>();

        // Decode property texture into the |property| object via a temporary
        // |material| object. The texture will be moved to non-material texture
        // library after decoding.
        auto* const texture_library =
            &geometry->GetMaterialLibrary().MutableTextureLibrary();
        Material material(texture_library);
        const auto& container_object = value.Get<tinygltf::Value::Object>();
        DRACO_RETURN_IF_ERROR(DecodeTexture(key, TextureMap::GENERIC,
                                            container_object, &material));
        property->SetTextureMap(
            *material.GetTextureMapByType(TextureMap::GENERIC));

        // Decode array of texture channel indices.
        DRACO_ASSIGN_OR_RETURN(const auto channels,
                               DecodeTextureChannelIndices(property_object));
        property->SetTextureChannels(channels);

        // Add property to the property texture.
        property_texture->AddProperty(std::move(property));
      }
    }

    // Add property texture to structural metadata.
    geometry->GetStructuralMetadata().AddPropertyTexture(
        std::move(property_texture));
  }
  return OkStatus();
}

Status GltfDecoder::AddAnimationsToScene() {
  for (const auto& animation : gltf_model_.animations) {
    const AnimationIndex animation_index = scene_->AddAnimation();
    Animation* const encoder_animation = scene_->GetAnimation(animation_index);
    encoder_animation->SetName(animation.name);

    for (const tinygltf::AnimationChannel& channel : animation.channels) {
      // The target node is optional in KHR_animation_pointer extension.
      SceneNodeIndex scene_node_index(kInvalidSceneNodeIndex);
      if (channel.target_node >= 0) {
        // If target node is valid, find the corresponding Draco scene node.
        const auto it = gltf_node_to_scenenode_index_.find(channel.target_node);
        if (it == gltf_node_to_scenenode_index_.end()) {
          return Status(Status::DRACO_ERROR,
                        "Could not find Node in the scene.");
        }
        scene_node_index = it->second;
      }
      // Add the channel to the animation. Note that scene_node_index can be
      // invalid (value is -1) which is handled correctly by the utility
      // function for KHR_animation_pointer.
      DRACO_RETURN_IF_ERROR(TinyGltfUtils::AddChannelToAnimation(
          gltf_model_, animation, channel, scene_node_index.value(),
          encoder_animation));
    }
  }
  return OkStatus();
}

Status GltfDecoder::DecodeNodeForScene(int node_index,
                                       SceneNodeIndex parent_index) {
  if (node_index < 0 || node_index >= gltf_model_.nodes.size()) {
    return Status(Status::DRACO_ERROR, "Node index out of bounds.");
  }
  SceneNodeIndex scene_node_index = kInvalidSceneNodeIndex;
  SceneNode* scene_node = nullptr;
  bool is_new_node;
  if (gltf_scene_graph_mode_ == GltfSceneGraphMode::DAG &&
      gltf_node_to_scenenode_index_.find(node_index) !=
          gltf_node_to_scenenode_index_.end()) {
    // Node has been decoded already.
    scene_node_index = gltf_node_to_scenenode_index_[node_index];
    scene_node = scene_->GetNode(scene_node_index);
    is_new_node = false;
  } else {
    scene_node_index = scene_->AddNode();
    // Update mapping between glTF Nodes and indices in the scene.
    gltf_node_to_scenenode_index_[node_index] = scene_node_index;

    scene_node = scene_->GetNode(scene_node_index);
    is_new_node = true;
  }

  if (parent_index != kInvalidSceneNodeIndex) {
    scene_node->AddParentIndex(parent_index);
    SceneNode* const parent_node = scene_->GetNode(parent_index);
    parent_node->AddChildIndex(scene_node_index);
  }

  if (!is_new_node) {
    return OkStatus();
  }
  const tinygltf::Node& node = gltf_model_.nodes[node_index];
  if (!node.name.empty()) {
    scene_node->SetName(node.name);
  }
  std::unique_ptr<TrsMatrix> trsm = GetNodeTrsMatrix(node);
  scene_node->SetTrsMatrix(*trsm);
  if (node.skin >= 0) {
    // Save the index to the source skins in the node. This will be updated
    // later when the skins are processed.
    scene_node->SetSkinIndex(SkinIndex(node.skin));
  }
  if (node.mesh >= 0) {
    if (node.mesh >= gltf_model_.meshes.size()) {
      return Status(Status::DRACO_ERROR, "Mesh index out of bounds.");
    }
    // Check if we have already parsed this glTF Mesh.
    const auto it = gltf_mesh_to_scene_mesh_group_.find(node.mesh);
    if (it != gltf_mesh_to_scene_mesh_group_.end()) {
      // We already processed this glTF mesh.
      scene_node->SetMeshGroupIndex(it->second);
    } else {
      const MeshGroupIndex scene_mesh_group_index = scene_->AddMeshGroup();
      MeshGroup* const scene_mesh =
          scene_->GetMeshGroup(scene_mesh_group_index);

      const tinygltf::Mesh& mesh = gltf_model_.meshes[node.mesh];
      if (!mesh.name.empty()) {
        scene_mesh->SetName(mesh.name);
      }
      for (const auto& primitive : mesh.primitives) {
        DRACO_RETURN_IF_ERROR(DecodePrimitiveForScene(primitive, scene_mesh));
      }
      if (!mesh.weights.empty()) {
        // Decode mesh groups's morph target weights.
        for (const auto weight : mesh.weights) {
          scene_mesh->AddMorphTargetWeight(weight);
        }
      }
      scene_node->SetMeshGroupIndex(scene_mesh_group_index);
      gltf_mesh_to_scene_mesh_group_[node.mesh] = scene_mesh_group_index;
    }
  }

  // Decode node's morph target weights.
  if (!node.weights.empty()) {
    for (const auto weight : node.weights) {
      scene_node->AddMorphTargetWeight(weight);
    }
  }

  // Decode light index.
  const auto& e = node.extensions.find("KHR_lights_punctual");
  if (e != node.extensions.end()) {
    const tinygltf::Value::Object& o = e->second.Get<tinygltf::Value::Object>();
    const auto& light = o.find("light");
    if (light != o.end()) {
      const tinygltf::Value& value = light->second;
      if (!value.IsInt()) {
        return ErrorStatus("Node light index is malformed.");
      }
      const int light_index = value.Get<int>();
      if (light_index < 0 || light_index >= scene_->NumLights()) {
        return ErrorStatus("Node light index is out of bounds.");
      }
      scene_node->SetLightIndex(LightIndex(light_index));
    }
  }

  for (int i = 0; i < node.children.size(); ++i) {
    DRACO_RETURN_IF_ERROR(
        DecodeNodeForScene(node.children[i], scene_node_index));
  }
  return OkStatus();
}

Status GltfDecoder::DecodePrimitiveForScene(
    const tinygltf::Primitive& primitive, MeshGroup* mesh_group) {
  if (primitive.mode != TINYGLTF_MODE_TRIANGLES &&
      primitive.mode != TINYGLTF_MODE_POINTS) {
    return Status(Status::DRACO_ERROR,
                  "Primitive does not contain triangles or points.");
  }

  // Decode materials variants mappings if present in this primitive.
  std::vector<MeshGroup::MaterialsVariantsMapping> mappings;
  const auto& e = primitive.extensions.find("KHR_materials_variants");
  if (e != primitive.extensions.end()) {
    DRACO_RETURN_IF_ERROR(DecodeMaterialsVariantsMappings(
        e->second.Get<tinygltf::Value::Object>(), &mappings));
  }

  const PrimitiveSignature signature(primitive);
  const auto existing_mesh_index =
      gltf_primitive_to_draco_mesh_index_.find(signature);
  if (existing_mesh_index != gltf_primitive_to_draco_mesh_index_.end()) {
    mesh_group->AddMeshInstance(
        {existing_mesh_index->second, primitive.material, mappings});
    return OkStatus();
  }

  // Mesh to be created from the primitive.
  std::unique_ptr<Mesh> mesh;

#ifdef DRACO_PMC_SUPPORTED
  // Check whether the mesh is encoded with PMC.
  const auto& extension = primitive.extensions.find("KHR_pmc_mesh_compression");
  if (mesh == nullptr && extension != primitive.extensions.end()) {
    DRACO_ASSIGN_OR_RETURN(
        mesh, DecodePrimitiveFromPmc(primitive, extension->second));
  }
#endif  // DRACO_PMC_SUPPORTED

  if (mesh == nullptr) {
    DRACO_ASSIGN_OR_RETURN(mesh, DecodePrimitiveFromGltfBuffer(primitive));
  }

  // Decode extensions present in this primitive.
  DRACO_RETURN_IF_ERROR(AddPrimitiveExtensionsToDracoMesh(
      primitive, &scene_->GetMaterialLibrary().MutableTextureLibrary(),
      mesh.get()));

  const MeshIndex mesh_index = scene_->AddMesh(std::move(mesh));
  if (mesh_index == kInvalidMeshIndex) {
    return Status(Status::DRACO_ERROR, "Could not add Draco mesh to scene.");
  }
  const int material_index = primitive.material;
  mesh_group->AddMeshInstance({mesh_index, material_index, mappings});

  gltf_primitive_to_draco_mesh_index_[signature] = mesh_index;
  return OkStatus();
}

#ifdef DRACO_PMC_SUPPORTED
StatusOr<std::unique_ptr<Mesh>> GltfDecoder::DecodePrimitiveFromPmc(
    const tinygltf::Primitive& primitive,
    const tinygltf::Value& pmc_extension) {
  auto buffer_view_value = pmc_extension.Get("bufferView");
  if (!buffer_view_value.IsInt()) {
    return ErrorStatus("PMC extension is malformed.");
  }
  const int buffer_view_index = buffer_view_value.Get<int>();
  if (buffer_view_index < 0 ||
      buffer_view_index >= gltf_model_.bufferViews.size()) {
    return ErrorStatus("PMC bufferView index is out of bounds.");
  }
  tinygltf::BufferView& view = gltf_model_.bufferViews[buffer_view_index];
  tinygltf::Buffer& buffer = gltf_model_.buffers[view.buffer];
  if (view.dracoDecoded) {
    // This PMC buffer has already been decoded.
    // TODO(vytyaz): Handle this case.
    return ErrorStatus("Multiple views of a PMC buffer not yet supported.");
  }
  view.dracoDecoded = true;

  const char* const buffer_view_data =
      reinterpret_cast<const char*>(buffer.data.data() + view.byteOffset);
  const size_t buffer_view_size = view.byteLength;

  // Decode mesh from PMC buffer.
  DecoderBuffer decoder_buffer;
  decoder_buffer.Init(buffer_view_data, buffer_view_size);
  PmcDracoMeshDecoder decoder;
  auto mesh = std::make_unique<Mesh>();
  std::vector<std::string> attribute_names;
  attribute_names.reserve(primitive.attributes.size());
  for (const auto& item : primitive.attributes) {
    attribute_names.push_back(item.first);
  }
  DRACO_RETURN_IF_ERROR(
      decoder.Decode(&decoder_buffer, mesh.get(), attribute_names));
  return mesh;
}
#endif  // DRACO_PMC_SUPPORTED

StatusOr<std::unique_ptr<Mesh>> GltfDecoder::DecodePrimitiveFromGltfBuffer(
    const tinygltf::Primitive& primitive) {
  const bool has_morph_targets = !primitive.targets.empty();
  const bool has_joints = HasJoints(primitive);

  // Handle indices first.
  DRACO_ASSIGN_OR_RETURN(const std::vector<uint32_t> indices_data,
                         DecodePrimitiveIndices(primitive));
  const int number_of_faces = indices_data.size() / 3;
  const int number_of_points = indices_data.size();

  // Note that glTF mesh |primitive| has no name; no name is set to Draco
  // mesh.
  TriangleSoupMeshBuilder mb;
  PointCloudBuilder pb;

  bool use_mesh_builder = true;
  if (primitive.mode != TINYGLTF_MODE_TRIANGLES || has_morph_targets ||
      has_joints) {
    // The decoded geometry is either a point cloud or it has morph targets or
    // joints in which case we use point cloud decoding to preserve input
    // indices. In case of morph targets, faces will be added back into the
    // decoded geometry in a later step.
    use_mesh_builder = false;
  }
  if (use_mesh_builder) {
    mb.Start(number_of_faces);
  } else {
    pb.Start(number_of_points);
  }

  bool deduplicate_vertices = deduplicate_vertices_;
  if (has_morph_targets || has_joints) {
    // We can't deduplicate vertices if there are morph targets or joints
    // because that could change indices.
    deduplicate_vertices = false;
  }

  // Clear attribute indices before populating attributes in |mb| or |pb|.
  feature_id_attribute_indices_.clear();

  DRACO_ASSIGN_OR_RETURN(
      std::unique_ptr<Mesh> mesh,
      AddAttributesAndBuildMesh(primitive.attributes, use_mesh_builder, false,
                                &mb, &pb, deduplicate_vertices, indices_data,
                                number_of_faces, number_of_points));

  if (has_morph_targets || has_joints) {
    // For morph targets and joints, the primitive was decoded as a point
    // cloud. We need to add the faces manually.
    for (int i = 0; i < number_of_faces; ++i) {
      Mesh::Face f;
      f[0] = indices_data[i * 3];
      f[1] = indices_data[i * 3 + 1];
      f[2] = indices_data[i * 3 + 2];
      mesh->AddFace(f);
    }

    // Decode morph targets.
    for (const auto& target : primitive.targets) {
      PointCloudBuilder morph_target_pb;
      morph_target_pb.Start(number_of_points);

      // Build morph target point cloud.
      DRACO_ASSIGN_OR_RETURN(
          std::unique_ptr<Mesh> morph_target_pc,
          AddAttributesAndBuildMesh(target, false, true, &mb, &morph_target_pb,
                                    false, indices_data, number_of_faces,
                                    number_of_points));

      // Assign morph target to the primitive.
      mesh->AddMorphTarget(std::move(morph_target_pc));
    }
  }
  return mesh;
}

StatusOr<std::unique_ptr<Mesh>> GltfDecoder::AddAttributesAndBuildMesh(
    const std::map<std::string, int>& attribute_name_to_accessor_map,
    bool use_mesh_builder, bool is_morph_target, TriangleSoupMeshBuilder* mb,
    PointCloudBuilder* pb, bool deduplicate_vertices,
    const std::vector<uint32_t>& indices_data, int number_of_faces,
    int number_of_points) {
  std::set<int32_t> normalized_attributes;
  for (const auto& attribute : attribute_name_to_accessor_map) {
    if (attribute.second >= gltf_model_.accessors.size()) {
      return ErrorStatus("Invalid accessor.");
    }
    const tinygltf::Accessor& accessor =
        gltf_model_.accessors[attribute.second];
    const int component_type = accessor.componentType;
    const int type = accessor.type;
    const bool normalized = accessor.normalized;
    int att_id = -1;
    if (use_mesh_builder) {
      DRACO_ASSIGN_OR_RETURN(
          att_id,
          AddAttribute(attribute.first, component_type, type, normalized, mb));
    } else {
      DRACO_ASSIGN_OR_RETURN(
          att_id,
          AddAttribute(attribute.first, component_type, type, normalized, pb));
    }
    if (att_id == -1) {
      continue;
    }
    if (normalized) {
      normalized_attributes.insert(att_id);
    }

    if (use_mesh_builder) {
      DRACO_RETURN_IF_ERROR(AddAttributeValuesToBuilder(
          attribute.first, accessor, indices_data, att_id, number_of_faces,
          Eigen::Matrix4d::Identity(), is_morph_target, mb));
    } else {
      DRACO_RETURN_IF_ERROR(AddAttributeValuesToBuilder(
          attribute.first, accessor, indices_data, att_id, number_of_points,
          Eigen::Matrix4d::Identity(), is_morph_target, pb));
    }
  }

  DRACO_ASSIGN_OR_RETURN(
      std::unique_ptr<Mesh> mesh,
      BuildMeshFromBuilder(use_mesh_builder, mb, pb, deduplicate_vertices));

  // Set all normalized flags for appropriate attributes.
  for (const int32_t att_id : normalized_attributes) {
    mesh->attribute(att_id)->set_normalized(true);
  }
  return mesh;
}

Status GltfDecoder::DecodeMaterialsVariantsMappings(
    const tinygltf::Value::Object& extension,
    std::vector<MeshGroup::MaterialsVariantsMapping>* mappings) {
  // Decode all materials variants mappings from JSON like this:
  //   "KHR_materials_variants" : {
  //     "mappings": [
  //       {
  //         "material": 2,
  //         "variants": [0, 2, 4]
  //       },
  //       {
  //         "material": 3,
  //         "variants": [1, 3]
  //       }
  //     ]
  //   }
  const auto& mappings_object = extension.find("mappings");
  if (mappings_object == extension.end()) {
    return ErrorStatus("Materials variants extension is malformed.");
  }
  const tinygltf::Value& mappings_array = mappings_object->second;
  if (!mappings_array.IsArray()) {
    return ErrorStatus("Materials variants mappings array is malformed.");
  }
  for (int i = 0; i < mappings_array.Size(); i++) {
    const auto& mapping_object = mappings_array.Get(i);
    if (!mapping_object.IsObject() || !mapping_object.Has("material") ||
        !mapping_object.Has("variants")) {
      return ErrorStatus("Materials variants mapping is malformed.");
    }
    const tinygltf::Value& material_int = mapping_object.Get("material");
    if (!material_int.IsInt()) {
      return ErrorStatus("Materials variant mapping material is malformed.");
    }
    const int material = material_int.Get<int>();
    const tinygltf::Value& variants_array = mapping_object.Get("variants");
    if (!variants_array.IsArray()) {
      return ErrorStatus("Materials variant mapping variants is malformed.");
    }
    std::vector<int> variants;
    for (int j = 0; j < variants_array.Size(); j++) {
      const tinygltf::Value& variant_int = variants_array.Get(j);
      if (!variant_int.IsInt()) {
        return ErrorStatus("Materials variants mapping variant is malformed.");
      }
      variants.push_back(variant_int.Get<int>());
    }
    mappings->push_back({material, variants});
  }
  return OkStatus();
}

Status GltfDecoder::DecodeMeshFeatures(const tinygltf::Primitive& primitive,
                                       TextureLibrary* texture_library,
                                       Mesh* mesh) {
  const auto& e = primitive.extensions.find("EXT_mesh_features");
  if (e == primitive.extensions.end()) {
    return OkStatus();
  }
  std::vector<std::unique_ptr<MeshFeatures>> mesh_features;
  DRACO_RETURN_IF_ERROR(
      DecodeMeshFeatures(e->second.Get<tinygltf::Value::Object>(),
                         texture_library, &mesh_features));
  for (int i = 0; i < mesh_features.size(); i++) {
    const MeshFeaturesIndex mfi =
        mesh->AddMeshFeatures(std::move(mesh_features[i]));
    if (scene_ == nullptr) {
      // If we are decoding to a mesh, we need to restrict the mesh features to
      // the primitive's material.
      // TODO(ostava): This will not work properly when two primitives share the
      // same material but have different mesh features. We will need to
      // duplicate the materials in this case.
      const auto mat_it =
          gltf_primitive_material_to_draco_material_.find(primitive.material);
      if (mat_it != gltf_primitive_material_to_draco_material_.end()) {
        mesh->AddMeshFeaturesMaterialMask(mfi, mat_it->second);
      }
    }
  }
  return OkStatus();
}

Status GltfDecoder::DecodeStructuralMetadata(
    const tinygltf::Primitive& primitive, Mesh* mesh) {
  const auto& e = primitive.extensions.find("EXT_structural_metadata");
  if (e == primitive.extensions.end()) {
    return OkStatus();
  }

  // Decode structural metadata from glTF primitive.
  std::vector<int> property_attributes_indices;
  std::vector<int> property_textures_indices;
  DRACO_RETURN_IF_ERROR(DecodeStructuralMetadata(
      e->second.Get<tinygltf::Value::Object>(), &property_attributes_indices,
      &property_textures_indices));

  // Process property attribute indices.
  for (const int pai : property_attributes_indices) {
    const int index = mesh->AddPropertyAttributesIndex(pai);
    if (scene_ == nullptr) {
      // If we are decoding to a mesh, we need to restrict the property
      // attributes indices to the primitive's material.
      // TODO(ostava): This will not work properly when two primitives share the
      // same material but have different property attributes indices. We will
      // need to duplicate the materials in this case.
      const auto mat_it =
          gltf_primitive_material_to_draco_material_.find(primitive.material);
      if (mat_it != gltf_primitive_material_to_draco_material_.end()) {
        mesh->AddPropertyAttributesIndexMaterialMask(index, mat_it->second);
      }
    }
  }

  // Process property texture indices.
  for (const int pti : property_textures_indices) {
    const int index = mesh->AddPropertyTexturesIndex(pti);
    if (scene_ == nullptr) {
      // If we are decoding to a mesh, we need to restrict the property
      // textures indices to the primitive's material.
      // TODO(ostava): This will not work properly when two primitives share the
      // same material but have different property textures indices. We will
      // need to duplicate the materials in this case.
      const auto mat_it =
          gltf_primitive_material_to_draco_material_.find(primitive.material);
      if (mat_it != gltf_primitive_material_to_draco_material_.end()) {
        mesh->AddPropertyTexturesIndexMaterialMask(index, mat_it->second);
      }
    }
  }
  return OkStatus();
}

Status GltfDecoder::DecodeMeshFeatures(
    const tinygltf::Value::Object& extension, TextureLibrary* texture_library,
    std::vector<std::unique_ptr<MeshFeatures>>* mesh_features) {
  // Decode all mesh feature ID sets from JSON like this:
  //   "EXT_mesh_features": {
  //     "featureIds": [
  //       {
  //         "label": "water",
  //         "featureCount": 2,
  //         "propertyTable": 0,
  //         "attribute": 0
  //       },
  //       {
  //         "featureCount": 12,
  //         "nullFeatureId": 100,
  //         "texture" : {
  //           "index": 0,
  //           "texCoord": 0,
  //           "channels": [0, 1, 2, 3]
  //         }
  //       }
  //     ]
  //   }
  const auto& object = extension.find("featureIds");
  if (object == extension.end()) {
    return ErrorStatus("Mesh features extension is malformed.");
  }
  const tinygltf::Value& array = object->second;
  if (!array.IsArray()) {
    return ErrorStatus("Mesh features array is malformed.");
  }
  for (int i = 0; i < array.Size(); i++) {
    // Create a new feature ID set object and populate it below.
    mesh_features->push_back(std::unique_ptr<MeshFeatures>(new MeshFeatures()));
    MeshFeatures& features = *mesh_features->back();

    const auto& object = array.Get(i);
    if (!object.IsObject()) {
      return ErrorStatus("Mesh features array entry is malformed.");
    }

    // The "featureCount" property is required.
    {
      constexpr char kName[] = "featureCount";
      if (!object.Has(kName)) {
        return ErrorStatus("Mesh features is malformed.");
      }
      const tinygltf::Value& value = object.Get(kName);
      if (!value.IsInt()) {
        return ErrorStatus("Feature count property is malformed.");
      }
      features.SetFeatureCount(value.Get<int>());
    }

    // All other properties are optional.
    {
      constexpr char kName[] = "nullFeatureId";
      if (object.Has(kName)) {
        const tinygltf::Value& value = object.Get(kName);
        if (!value.IsInt()) {
          return ErrorStatus("Null feature ID property is malformed.");
        }
        features.SetNullFeatureId(value.Get<int>());
      }
    }
    {
      constexpr char kName[] = "label";
      if (object.Has(kName)) {
        const tinygltf::Value& value = object.Get(kName);
        if (!value.IsString()) {
          return ErrorStatus("Label property is malformed.");
        }
        features.SetLabel(value.Get<std::string>());
      }
    }
    {
      constexpr char kName[] = "attribute";
      if (object.Has(kName)) {
        const tinygltf::Value& value = object.Get(kName);
        if (!value.IsInt()) {
          return ErrorStatus("Attribute property is malformed.");
        }
        // Convert index in feature ID vertex attribute name like _FEATURE_ID_5
        // to attribute index in draco::Mesh.
        const int att_name_index = value.Get<int>();
        const int att_index = feature_id_attribute_indices_[att_name_index];
        features.SetAttributeIndex(att_index);
      }
    }
    {
      constexpr char kName[] = "texture";
      if (object.Has(kName)) {
        const tinygltf::Value& value = object.Get(kName);
        if (!value.IsObject()) {
          return ErrorStatus("Texture property is malformed.");
        }

        // Decode texture containing mesh feature IDs into the |features| object
        // via a temporary |material| object. The texture will be moved to
        // non-material texture library after decoding.
        Material material(texture_library);
        const auto& container_object = object.Get<tinygltf::Value::Object>();
        DRACO_RETURN_IF_ERROR(DecodeTexture(kName, TextureMap::GENERIC,
                                            container_object, &material));
        features.SetTextureMap(
            *material.GetTextureMapByType(TextureMap::GENERIC));

        // Decode array of texture channel indices.
        DRACO_ASSIGN_OR_RETURN(const auto channels,
                               DecodeTextureChannelIndices(value));
        features.SetTextureChannels(channels);
      }
    }
    {
      constexpr char kName[] = "propertyTable";
      if (object.Has(kName)) {
        const tinygltf::Value& value = object.Get(kName);
        if (!value.IsInt()) {
          return ErrorStatus("Property table property is malformed.");
        }
        features.SetPropertyTableIndex(value.Get<int>());
      }
    }
  }
  return OkStatus();
}

Status GltfDecoder::DecodeStructuralMetadata(
    const tinygltf::Value::Object& extension,
    std::vector<int>* property_attributes,
    std::vector<int>* property_textures) {
  // Decode all structural metadata from JSON like this in glTF primitive:
  //   "EXT_structural_metadata": {
  //     "propertyAttributes": [0],
  //     "propertyTextures": [0]
  //   }

  // Decode property attributes in glTF primitive.
  const auto& attributes_object = extension.find("propertyAttributes");
  if (attributes_object != extension.end()) {
    const tinygltf::Value& array = attributes_object->second;
    if (!array.IsArray()) {
      return ErrorStatus("Property attributes array is malformed.");
    }
    for (int i = 0; i < array.Size(); i++) {
      const auto& value = array.Get(i);
      if (!value.IsInt()) {
        return ErrorStatus("Property attributes array entry is malformed.");
      }
      property_attributes->push_back(value.Get<int>());
    }
  }

  // Decode property textures in glTF primitive.
  const auto& textures_object = extension.find("propertyTextures");
  if (textures_object != extension.end()) {
    const tinygltf::Value& array = textures_object->second;
    if (!array.IsArray()) {
      return ErrorStatus("Property textures array is malformed.");
    }
    for (int i = 0; i < array.Size(); i++) {
      const auto& value = array.Get(i);
      if (!value.IsInt()) {
        return ErrorStatus("Property textures array entry is malformed.");
      }
      property_textures->push_back(value.Get<int>());
    }
  }
  return OkStatus();
}

template <typename BuilderT>
StatusOr<int> GltfDecoder::AddAttribute(const std::string& attribute_name,
                                        int component_type, int type,
                                        bool normalized, BuilderT* builder) {
  const GeometryAttribute::Type draco_att_type =
      GltfAttributeToDracoAttribute(attribute_name);
  if (draco_att_type == GeometryAttribute::INVALID) {
    // Return attribute id -1 that will be ignored and not included in the mesh.
    return -1;
  }
  DRACO_ASSIGN_OR_RETURN(
      const int att_id,
      AddAttribute(draco_att_type, component_type, type, normalized, builder));

  if (draco_att_type == draco::GeometryAttribute::GENERIC) {
    AddAppSpecificAttrName(att_id, attribute_name, builder);
  }
  return att_id;
}

template <typename BuilderT>
StatusOr<int> GltfDecoder::AddAttribute(GeometryAttribute::Type attribute_type,
                                        int component_type, int type,
                                        bool normalized, BuilderT* builder) {
  const int num_components = TinyGltfUtils::GetNumComponentsForType(type);
  if (num_components == 0) {
    return Status(Status::DRACO_ERROR,
                  "Could not add attribute with 0 components.");
  }

  const draco::DataType draco_component_type =
      GltfComponentTypeToDracoType(component_type);
  if (draco_component_type == DT_INVALID) {
    return Status(Status::DRACO_ERROR,
                  "Could not add attribute with invalid type.");
  }
  const int att_id = builder->AddAttribute(attribute_type, num_components,
                                           draco_component_type, normalized);
  if (att_id < 0) {
    return Status(Status::DRACO_ERROR, "Could not add attribute.");
  }

  // When glTF is loaded as a mesh, initialize color attribute values to white
  // opaque color. Mesh regions corresponding to glTF primitives without vertex
  // color will end up having the white color. Since vertex color is used as a
  // multiplier for material base color in rendering shader, the white color
  // will keep the model appearance unchanged.
  if (scene_ == nullptr && attribute_type == GeometryAttribute::Type::COLOR) {
    SetWhiteVertexColor(att_id, draco_component_type, builder);
  }
  return att_id;
}

template <typename BuilderT>
void GltfDecoder::SetWhiteVertexColor(int color_att_id, draco::DataType type,
                                      BuilderT* builder) {
  // Valid glTF vertex color types are float, unsigned byte, and unsigned short.
  if (type == DT_FLOAT32) {
    SetWhiteVertexColorOfType<float>(color_att_id, builder);
  } else if (type == DT_UINT8) {
    SetWhiteVertexColorOfType<uint8_t>(color_att_id, builder);
  } else if (type == DT_UINT16) {
    SetWhiteVertexColorOfType<uint16_t>(color_att_id, builder);
  }
}

template <typename ComponentT>
void GltfDecoder::SetWhiteVertexColorOfType(int color_att_id,
                                            TriangleSoupMeshBuilder* builder) {
  // The alpha component will not be copied for the RGB vertex colors.
  std::array<ComponentT, 4> white{1, 1, 1, 1};
  const int num_faces = total_face_indices_count_ / 3;
  for (FaceIndex fi(0); fi < num_faces; fi++) {
    builder->SetAttributeValuesForFace(color_att_id, fi, white.data(),
                                       white.data(), white.data());
  }
}

template <typename ComponentT>
void GltfDecoder::SetWhiteVertexColorOfType(int color_att_id,
                                            PointCloudBuilder* builder) {
  // The alpha component will not be copied for the RGB vertex colors.
  std::array<ComponentT, 4> white{1, 1, 1, 1};
  const int num_points = total_point_indices_count_;
  for (PointIndex pi(0); pi < num_points; pi++) {
    builder->SetAttributeValueForPoint(color_att_id, pi, white.data());
  }
}

StatusOr<bool> GltfDecoder::CheckKhrTextureTransform(
    const tinygltf::ExtensionMap& extension, TextureTransform* transform) {
  bool transform_set = false;

  const auto& e = extension.find("KHR_texture_transform");
  if (e == extension.end()) {
    return false;
  }
  const tinygltf::Value::Object& o = e->second.Get<tinygltf::Value::Object>();
  const auto& scale = o.find("scale");
  if (scale != o.end()) {
    const tinygltf::Value& array = scale->second;
    if (!array.IsArray() || array.Size() != 2) {
      return Status(Status::DRACO_ERROR,
                    "KhrTextureTransform scale is malformed.");
    }
    std::array<double, 2> scale;
    for (int i = 0; i < array.Size(); i++) {
      const tinygltf::Value& value = array.Get(i);
      if (!value.IsNumber()) {
        return Status(Status::DRACO_ERROR,
                      "KhrTextureTransform scale is malformed.");
      }
      scale[i] = value.Get<double>();
      transform_set = true;
    }
    transform->set_scale(scale);
  }
  const auto& rotation = o.find("rotation");
  if (rotation != o.end()) {
    const tinygltf::Value& value = rotation->second;
    if (!value.IsNumber()) {
      return Status(Status::DRACO_ERROR,
                    "KhrTextureTransform rotation is malformed.");
    }
    transform->set_rotation(value.Get<double>());
    transform_set = true;
  }
  const auto& offset = o.find("offset");
  if (offset != o.end()) {
    const tinygltf::Value& array = offset->second;
    if (!array.IsArray() || array.Size() != 2) {
      return Status(Status::DRACO_ERROR,
                    "KhrTextureTransform offset is malformed.");
    }
    std::array<double, 2> offset;
    for (int i = 0; i < array.Size(); i++) {
      const tinygltf::Value& value = array.Get(i);
      if (!value.IsNumber()) {
        return Status(Status::DRACO_ERROR,
                      "KhrTextureTransform offset is malformed.");
      }
      offset[i] = value.Get<double>();
      transform_set = true;
    }
    transform->set_offset(offset);
  }
  const auto& tex_coord = o.find("texCoord");
  if (tex_coord != o.end()) {
    const tinygltf::Value& value = tex_coord->second;
    if (!value.IsInt()) {
      return Status(Status::DRACO_ERROR,
                    "KhrTextureTransform texCoord is malformed.");
    }
    transform->set_tex_coord(value.Get<int>());
    transform_set = true;
  }
  return transform_set;
}

Status GltfDecoder::AddGltfMaterial(int input_material_index,
                                    Material* output_material) {
  const tinygltf::Material& input_material =
      gltf_model_.materials[input_material_index];

  output_material->SetName(input_material.name);
  output_material->SetTransparencyMode(
      TinyGltfUtils::TextToMaterialMode(input_material.alphaMode));
  output_material->SetAlphaCutoff(input_material.alphaCutoff);
  if (input_material.emissiveFactor.size() == 3) {
    output_material->SetEmissiveFactor(Vector3f(
        input_material.emissiveFactor[0], input_material.emissiveFactor[1],
        input_material.emissiveFactor[2]));
  }
  const tinygltf::PbrMetallicRoughness& pbr =
      input_material.pbrMetallicRoughness;

  if (pbr.baseColorFactor.size() == 4) {
    output_material->SetColorFactor(
        Vector4f(pbr.baseColorFactor[0], pbr.baseColorFactor[1],
                 pbr.baseColorFactor[2], pbr.baseColorFactor[3]));
  }
  output_material->SetMetallicFactor(pbr.metallicFactor);
  output_material->SetRoughnessFactor(pbr.roughnessFactor);
  output_material->SetDoubleSided(input_material.doubleSided);

  DRACO_RETURN_IF_ERROR(CheckAndAddTextureToDracoMaterial(
      pbr.baseColorTexture.index, pbr.baseColorTexture.texCoord,
      pbr.baseColorTexture.extensions, output_material, TextureMap::COLOR));
  DRACO_RETURN_IF_ERROR(CheckAndAddTextureToDracoMaterial(
      pbr.metallicRoughnessTexture.index, pbr.metallicRoughnessTexture.texCoord,
      pbr.metallicRoughnessTexture.extensions, output_material,
      TextureMap::METALLIC_ROUGHNESS));

  DRACO_RETURN_IF_ERROR(CheckAndAddTextureToDracoMaterial(
      input_material.normalTexture.index, input_material.normalTexture.texCoord,
      input_material.normalTexture.extensions, output_material,
      TextureMap::NORMAL_TANGENT_SPACE));
  if (input_material.normalTexture.scale != 1.0) {
    output_material->SetNormalTextureScale(input_material.normalTexture.scale);
  }
  DRACO_RETURN_IF_ERROR(CheckAndAddTextureToDracoMaterial(
      input_material.occlusionTexture.index,
      input_material.occlusionTexture.texCoord,
      input_material.occlusionTexture.extensions, output_material,
      TextureMap::AMBIENT_OCCLUSION));
  DRACO_RETURN_IF_ERROR(CheckAndAddTextureToDracoMaterial(
      input_material.emissiveTexture.index,
      input_material.emissiveTexture.texCoord,
      input_material.emissiveTexture.extensions, output_material,
      TextureMap::EMISSIVE));

  // Decode material extensions.
  DecodeMaterialUnlitExtension(input_material, output_material);
  DRACO_RETURN_IF_ERROR(
      DecodeMaterialSheenExtension(input_material, output_material));
  DRACO_RETURN_IF_ERROR(
      DecodeMaterialTransmissionExtension(input_material, output_material));
  DRACO_RETURN_IF_ERROR(
      DecodeMaterialClearcoatExtension(input_material, output_material));
  DRACO_RETURN_IF_ERROR(DecodeMaterialVolumeExtension(
      input_material, input_material_index, output_material));
  DRACO_RETURN_IF_ERROR(
      DecodeMaterialIorExtension(input_material, output_material));
  DRACO_RETURN_IF_ERROR(
      DecodeMaterialSpecularExtension(input_material, output_material));
  DRACO_RETURN_IF_ERROR(
      DecodeMaterialEmissiveStrengthExtension(input_material, output_material));
  DRACO_RETURN_IF_ERROR(
      DecodeMaterialIridescenceExtension(input_material, output_material));
  DRACO_RETURN_IF_ERROR(
      DecodeMaterialAnisotropyExtension(input_material, output_material));
  DRACO_RETURN_IF_ERROR(DecodeMaterialDiffuseTransmissionExtension(
      input_material, output_material));

  return OkStatus();
}

void GltfDecoder::DecodeMaterialUnlitExtension(
    const tinygltf::Material& input_material, Material* output_material) {
  // Do nothing if extension is absent.
  const auto& extension_it =
      input_material.extensions.find("KHR_materials_unlit");
  if (extension_it == input_material.extensions.end()) {
    return;
  }

  // Set the unlit property in Draco material.
  output_material->SetUnlit(true);
}

Status GltfDecoder::DecodeMaterialSheenExtension(
    const tinygltf::Material& input_material, Material* output_material) {
  // Do nothing if extension is absent.
  const auto& extension_it =
      input_material.extensions.find("KHR_materials_sheen");
  if (extension_it == input_material.extensions.end()) {
    return OkStatus();
  }

  output_material->SetHasSheen(true);
  const tinygltf::Value::Object& extension_object =
      extension_it->second.Get<tinygltf::Value::Object>();

  // Decode sheen color factor.
  Vector3f vector;
  bool success;
  DRACO_ASSIGN_OR_RETURN(
      success, DecodeVector3f("sheenColorFactor", extension_object, &vector));
  if (success) {
    output_material->SetSheenColorFactor(vector);
  }

  // Decode sheen roughness factor.
  float value;
  DRACO_ASSIGN_OR_RETURN(
      success, DecodeFloat("sheenRoughnessFactor", extension_object, &value));
  if (success) {
    output_material->SetSheenRoughnessFactor(value);
  }

  // Decode sheen color texture.
  DRACO_RETURN_IF_ERROR(DecodeTexture("sheenColorTexture",
                                      TextureMap::SHEEN_COLOR, extension_object,
                                      output_material));

  // Decode sheen roughness texture.
  DRACO_RETURN_IF_ERROR(DecodeTexture("sheenRoughnessTexture",
                                      TextureMap::SHEEN_ROUGHNESS,
                                      extension_object, output_material));

  return OkStatus();
}

Status GltfDecoder::DecodeMaterialTransmissionExtension(
    const tinygltf::Material& input_material, Material* output_material) {
  // Do nothing if extension is absent.
  const auto& extension_it =
      input_material.extensions.find("KHR_materials_transmission");
  if (extension_it == input_material.extensions.end()) {
    return OkStatus();
  }

  output_material->SetHasTransmission(true);
  const tinygltf::Value::Object& extension_object =
      extension_it->second.Get<tinygltf::Value::Object>();

  // Decode transmission factor.
  float value;
  DRACO_ASSIGN_OR_RETURN(
      const bool success,
      DecodeFloat("transmissionFactor", extension_object, &value));
  if (success) {
    output_material->SetTransmissionFactor(value);
  }

  // Decode transmission texture.
  DRACO_RETURN_IF_ERROR(DecodeTexture("transmissionTexture",
                                      TextureMap::TRANSMISSION,
                                      extension_object, output_material));

  return OkStatus();
}

Status GltfDecoder::DecodeMaterialClearcoatExtension(
    const tinygltf::Material& input_material, Material* output_material) {
  // Do nothing if extension is absent.
  const auto& extension_it =
      input_material.extensions.find("KHR_materials_clearcoat");
  if (extension_it == input_material.extensions.end()) {
    return OkStatus();
  }

  output_material->SetHasClearcoat(true);
  const tinygltf::Value::Object& extension_object =
      extension_it->second.Get<tinygltf::Value::Object>();

  // Decode clearcoat factor.
  float value;
  bool success;
  DRACO_ASSIGN_OR_RETURN(
      success, DecodeFloat("clearcoatFactor", extension_object, &value));
  if (success) {
    output_material->SetClearcoatFactor(value);
  }

  // Decode clearcoat roughness factor.
  DRACO_ASSIGN_OR_RETURN(success, DecodeFloat("clearcoatRoughnessFactor",
                                              extension_object, &value));
  if (success) {
    output_material->SetClearcoatRoughnessFactor(value);
  }

  // Decode clearcoat texture.
  DRACO_RETURN_IF_ERROR(DecodeTexture("clearcoatTexture", TextureMap::CLEARCOAT,
                                      extension_object, output_material));

  // Decode clearcoat roughness texture.
  DRACO_RETURN_IF_ERROR(DecodeTexture("clearcoatRoughnessTexture",
                                      TextureMap::CLEARCOAT_ROUGHNESS,
                                      extension_object, output_material));

  // Decode clearcoat normal texture.
  DRACO_RETURN_IF_ERROR(DecodeTexture("clearcoatNormalTexture",
                                      TextureMap::CLEARCOAT_NORMAL,
                                      extension_object, output_material));

  return OkStatus();
}

Status GltfDecoder::DecodeMaterialVolumeExtension(
    const tinygltf::Material& input_material, int input_material_index,
    Material* output_material) {
  // Do nothing if extension is absent.
  const auto& extension_it =
      input_material.extensions.find("KHR_materials_volume");
  if (extension_it == input_material.extensions.end()) {
    return OkStatus();
  }

  output_material->SetHasVolume(true);
  const tinygltf::Value::Object& extension_object =
      extension_it->second.Get<tinygltf::Value::Object>();

  // Decode thickness factor.
  float value;
  bool success;
  DRACO_ASSIGN_OR_RETURN(
      success, DecodeFloat("thicknessFactor", extension_object, &value));
  if (success) {
    // Volume thickness factor is given in the coordinate space of the model.
    // When the model is loaded as draco::Mesh, the scene graph transformations
    // are applied to position attribute. Since this effectively scales the
    // model coordinate space, the volume thickness factor also must be scaled.
    // No scaling is done when the model is loaded as draco::Scene.
    float scale = 1.0f;
    if (scene_ == nullptr) {
      if (gltf_primitive_material_to_scales_.count(input_material_index) == 1) {
        const std::vector<float>& scales =
            gltf_primitive_material_to_scales_[input_material_index];

        // It is only possible to scale the volume thickness factor if all
        // primitives using this material have the same transformation scale.
        // An alternative would be to create a separate meterial for each scale.
        scale = scales[0];
        for (int i = 1; i < scales.size(); i++) {
          // Note that close-enough scales could also be permitted.
          if (scales[i] != scale) {
            return ErrorStatus("Cannot represent volume thickness in a mesh.");
          }
        }
      }
    }
    output_material->SetThicknessFactor(scale * value);
  }

  // Decode attenuation distance.
  DRACO_ASSIGN_OR_RETURN(
      success, DecodeFloat("attenuationDistance", extension_object, &value));
  if (success) {
    output_material->SetAttenuationDistance(value);
  }

  // Decode attenuation color.
  Vector3f vector;
  DRACO_ASSIGN_OR_RETURN(
      success, DecodeVector3f("attenuationColor", extension_object, &vector));
  if (success) {
    output_material->SetAttenuationColor(vector);
  }

  // Decode thickness texture.
  DRACO_RETURN_IF_ERROR(DecodeTexture("thicknessTexture", TextureMap::THICKNESS,
                                      extension_object, output_material));

  return OkStatus();
}

Status GltfDecoder::DecodeMaterialIorExtension(
    const tinygltf::Material& input_material, Material* output_material) {
  // Do nothing if extension is absent.
  const auto& extension_it =
      input_material.extensions.find("KHR_materials_ior");
  if (extension_it == input_material.extensions.end()) {
    return OkStatus();
  }

  output_material->SetHasIor(true);
  const tinygltf::Value::Object& extension_object =
      extension_it->second.Get<tinygltf::Value::Object>();

  // Decode index of refraction.
  float value;
  DRACO_ASSIGN_OR_RETURN(const bool success,
                         DecodeFloat("ior", extension_object, &value));
  if (success) {
    output_material->SetIor(value);
  }

  return OkStatus();
}

Status GltfDecoder::DecodeMaterialSpecularExtension(
    const tinygltf::Material& input_material, Material* output_material) {
  // Do nothing if extension is absent.
  const auto& extension_it =
      input_material.extensions.find("KHR_materials_specular");
  if (extension_it == input_material.extensions.end()) {
    return OkStatus();
  }

  output_material->SetHasSpecular(true);
  const tinygltf::Value::Object& extension_object =
      extension_it->second.Get<tinygltf::Value::Object>();

  // Decode specular factor.
  float value;
  bool success;
  DRACO_ASSIGN_OR_RETURN(
      success, DecodeFloat("specularFactor", extension_object, &value));
  if (success) {
    output_material->SetSpecularFactor(value);
  }

  // Decode specular color factor.
  Vector3f vector;
  DRACO_ASSIGN_OR_RETURN(success, DecodeVector3f("specularColorFactor",
                                                 extension_object, &vector));
  if (success) {
    output_material->SetSpecularColorFactor(vector);
  }

  // Decode specular texture.
  DRACO_RETURN_IF_ERROR(DecodeTexture("specularTexture", TextureMap::SPECULAR,
                                      extension_object, output_material));

  // Decode specular color texture.
  DRACO_RETURN_IF_ERROR(DecodeTexture("specularColorTexture",
                                      TextureMap::SPECULAR_COLOR,
                                      extension_object, output_material));

  return OkStatus();
}

Status GltfDecoder::DecodeMaterialEmissiveStrengthExtension(
    const tinygltf::Material& input_material, Material* output_material) {
  // Do nothing if extension is absent.
  const auto& extension_it =
      input_material.extensions.find("KHR_materials_emissive_strength");
  if (extension_it == input_material.extensions.end()) {
    return OkStatus();
  }

  output_material->SetHasEmissiveStrength(true);

  const tinygltf::Value::Object& extension_object =
      extension_it->second.Get<tinygltf::Value::Object>();

  // Decode emissive strength value.
  float value;
  bool success;
  DRACO_ASSIGN_OR_RETURN(
      success, DecodeFloat("emissiveStrength", extension_object, &value));
  if (success) {
    output_material->SetEmissiveStrength(value);
  }

  return OkStatus();
}

Status GltfDecoder::DecodeMaterialIridescenceExtension(
    const tinygltf::Material& input_material, Material* output_material) {
  const auto& extension_it =
      input_material.extensions.find("KHR_materials_iridescence");
  if (extension_it == input_material.extensions.end()) {
    return OkStatus();
  }

  const tinygltf::Value::Object& extension_object =
      extension_it->second.Get<tinygltf::Value::Object>();

  float value;
  bool success;
  DRACO_ASSIGN_OR_RETURN(
      success, DecodeFloat("iridescenceFactor", extension_object, &value));
  if (success) {
    output_material->SetIridescenceFactor(value);
  }

  DRACO_ASSIGN_OR_RETURN(
      success, DecodeFloat("iridescenceIor", extension_object, &value));
  if (success) {
    output_material->SetIridescenceIor(value);
  }

  DRACO_ASSIGN_OR_RETURN(success, DecodeFloat("iridescenceThicknessMinimum",
                                              extension_object, &value));
  if (success) {
    output_material->SetIridescenceThicknessMinimum(value);
  }

  DRACO_ASSIGN_OR_RETURN(success, DecodeFloat("iridescenceThicknessMaximum",
                                              extension_object, &value));
  if (success) {
    output_material->SetIridescenceThicknessMaximum(value);
  }

  DRACO_RETURN_IF_ERROR(DecodeTexture("iridescenceTexture",
                                      TextureMap::IRIDESCENCE, extension_object,
                                      output_material));

  DRACO_RETURN_IF_ERROR(DecodeTexture("iridescenceThicknessTexture",
                                      TextureMap::IRIDESCENCE_THICKNESS,
                                      extension_object, output_material));

  output_material->SetHasIridescence(true);
  return OkStatus();
}

Status GltfDecoder::DecodeMaterialAnisotropyExtension(
    const tinygltf::Material& input_material, Material* output_material) {
  const auto& extension_it =
      input_material.extensions.find("KHR_materials_anisotropy");
  if (extension_it == input_material.extensions.end()) {
    return OkStatus();
  }

  const tinygltf::Value::Object& extension_object =
      extension_it->second.Get<tinygltf::Value::Object>();

  float value;
  bool success;
  DRACO_ASSIGN_OR_RETURN(
      success, DecodeFloat("anisotropyStrength", extension_object, &value));
  if (success) {
    output_material->SetAnisotropyStrength(value);
  }

  DRACO_ASSIGN_OR_RETURN(
      success, DecodeFloat("anisotropyRotation", extension_object, &value));
  if (success) {
    output_material->SetAnisotropyRotation(value);
  }

  DRACO_RETURN_IF_ERROR(DecodeTexture("anisotropyTexture",
                                      TextureMap::ANISOTROPY, extension_object,
                                      output_material));

  output_material->SetHasAnisotropy(true);
  return OkStatus();
}

Status GltfDecoder::DecodeMaterialDiffuseTransmissionExtension(
    const tinygltf::Material& input_material, Material* output_material) {
  const auto& extension_it =
      input_material.extensions.find("KHR_materials_diffuse_transmission");
  if (extension_it == input_material.extensions.end()) {
    return OkStatus();
  }

  const tinygltf::Value::Object& extension_object =
      extension_it->second.Get<tinygltf::Value::Object>();

  float value;
  bool success;
  DRACO_ASSIGN_OR_RETURN(success, DecodeFloat("diffuseTransmissionFactor",
                                              extension_object, &value));
  if (success) {
    output_material->SetDiffuseTransmissionFactor(value);
  }

  Vector3f vector;
  DRACO_ASSIGN_OR_RETURN(success,
                         DecodeVector3f("diffuseTransmissionColorFactor",
                                        extension_object, &vector));
  if (success) {
    output_material->SetDiffuseTransmissionColorFactor(vector);
  }

  DRACO_RETURN_IF_ERROR(DecodeTexture("diffuseTransmissionTexture",
                                      TextureMap::DIFFUSE_TRANSMISSION,
                                      extension_object, output_material));

  DRACO_RETURN_IF_ERROR(DecodeTexture("diffuseTransmissionColorTexture",
                                      TextureMap::DIFFUSE_TRANSMISSION_COLOR,
                                      extension_object, output_material));

  output_material->SetHasDiffuseTransmission(true);
  return OkStatus();
}

StatusOr<bool> GltfDecoder::DecodeFloat(const std::string& name,
                                        const tinygltf::Value::Object& object,
                                        float* value) {
  const auto& it = object.find(name);
  if (it == object.end()) {
    return false;
  }
  const tinygltf::Value& number = it->second;
  if (!number.IsNumber()) {
    return Status(Status::DRACO_ERROR, "Invalid " + name + ".");
  }
  *value = number.Get<double>();
  return true;
}

StatusOr<bool> GltfDecoder::DecodeInt(const std::string& name,
                                      const tinygltf::Value::Object& object,
                                      int* value) {
  const auto& it = object.find(name);
  if (it == object.end()) {
    return false;
  }
  const tinygltf::Value& number = it->second;
  if (!number.IsNumber()) {
    return ErrorStatus("Invalid " + name + ".");
  }
  *value = number.Get<int>();
  return true;
}

StatusOr<bool> GltfDecoder::DecodeString(const std::string& name,
                                         const tinygltf::Value::Object& object,
                                         std::string* value) {
  const auto& it = object.find(name);
  if (it == object.end()) {
    return false;
  }
  const tinygltf::Value& string = it->second;
  if (!string.IsString()) {
    return ErrorStatus("Invalid " + name + ".");
  }
  *value = string.Get<std::string>();
  return true;
}

StatusOr<std::vector<int>> GltfDecoder::DecodeTextureChannelIndices(
    const tinygltf::Value& value) {
  std::vector<int> channels;
  constexpr char kName[] = "channels";
  if (value.Has(kName)) {
    const tinygltf::Value& array = value.Get(kName);
    if (!array.IsArray()) {
      return ErrorStatus("Channels property is malformed.");
    }
    for (int i = 0; i < array.Size(); i++) {
      const tinygltf::Value& value = array.Get(i);
      if (!value.IsNumber()) {
        return Status(Status::DRACO_ERROR, "Channels value is malformed.");
      }
      channels.push_back(value.Get<int>());
    }
  } else {
    channels = {0};
  }
  return channels;
}

StatusOr<bool> GltfDecoder::DecodePropertyTableData(
    const std::string& name, const tinygltf::Value::Object& object,
    PropertyTable::Property::Data* data) {
  int buffer_view_index;
  DRACO_ASSIGN_OR_RETURN(const bool success,
                         DecodeInt(name, object, &buffer_view_index));
  if (!success) {
    return false;
  }
  DRACO_RETURN_IF_ERROR(
      CopyDataFromBufferView(gltf_model_, buffer_view_index, &data->data));
  data->target = gltf_model_.bufferViews[buffer_view_index].target;
  return true;
}

StatusOr<bool> GltfDecoder::DecodeVector3f(
    const std::string& name, const tinygltf::Value::Object& object,
    Vector3f* value) {
  const auto& it = object.find(name);
  if (it == object.end()) {
    return false;
  }
  const tinygltf::Value& array = it->second;
  if (!array.IsArray() || array.Size() != 3) {
    return Status(Status::DRACO_ERROR, "Invalid " + name + ".");
  }
  for (int i = 0; i < array.Size(); i++) {
    const tinygltf::Value& array_entry = array.Get(i);
    if (!array_entry.IsNumber()) {
      return Status(Status::DRACO_ERROR, "Invalid " + name + ".");
    }
    (*value)[i] = array_entry.Get<double>();
  }
  return true;
}

Status GltfDecoder::DecodeTexture(const std::string& name,
                                  TextureMap::Type type,
                                  const tinygltf::Value::Object& object,
                                  Material* material) {
  tinygltf::TextureInfo info;
  DRACO_RETURN_IF_ERROR(ParseTextureInfo(name, object, &info));
  DRACO_RETURN_IF_ERROR(CheckAndAddTextureToDracoMaterial(
      info.index, info.texCoord, info.extensions, material, type));
  return OkStatus();
}

Status GltfDecoder::ParseTextureInfo(
    const std::string& texture_name,
    const tinygltf::Value::Object& container_object,
    tinygltf::TextureInfo* texture_info) {
  // Note that tinygltf only parses material textures and not material extension
  // textures. This method mimics the behavior of tinygltf's private function
  // ParseTextureInfo() in order for Draco to decode extension textures.

  // Do nothing if texture with such name is absent.
  const auto& texture_object_it = container_object.find(texture_name);
  if (texture_object_it == container_object.end()) {
    return OkStatus();
  }

  const tinygltf::Value::Object& texture_object =
      texture_object_it->second.Get<tinygltf::Value::Object>();

  // Decode texture index.
  const auto& index_it = texture_object.find("index");
  if (index_it != texture_object.end()) {
    const tinygltf::Value& value = index_it->second;
    if (!value.IsNumber()) {
      return Status(Status::DRACO_ERROR, "Invalid texture index.");
    }
    texture_info->index = value.Get<int>();
  }

  // Decode texture coordinate index.
  const auto& tex_coord_it = texture_object.find("texCoord");
  if (tex_coord_it != texture_object.end()) {
    const tinygltf::Value& value = tex_coord_it->second;
    if (!value.IsInt()) {
      return Status(Status::DRACO_ERROR, "Invalid texture texCoord.");
    }
    texture_info->texCoord = value.Get<int>();
  }

  // Decode texture extensions.
  const auto& extensions_it = texture_object.find("extensions");
  if (extensions_it != texture_object.end()) {
    const tinygltf::Value& extensions = extensions_it->second;
    if (!extensions.IsObject()) {
      return Status(Status::DRACO_ERROR, "Invalid extension.");
    }
    for (const std::string& key : extensions.Keys()) {
      texture_info->extensions[key] = extensions.Get(key);
    }
  }

  // Decode texture extras.
  const auto& extras_it = texture_object.find("extras");
  if (extras_it != texture_object.end()) {
    texture_info->extras = extras_it->second;
  }

  return OkStatus();
}

Status GltfDecoder::AddMaterialsToScene() {
  for (int input_material_index = 0;
       input_material_index < gltf_model_.materials.size();
       ++input_material_index) {
    Material* const output_material =
        scene_->GetMaterialLibrary().MutableMaterial(input_material_index);
    DRACO_RETURN_IF_ERROR(
        AddGltfMaterial(input_material_index, output_material));
  }

  // Check if we need to add a default material for primitives without an
  // assigned material.
  const int default_material_index =
      scene_->GetMaterialLibrary().NumMaterials();
  bool default_material_needed = false;
  for (MeshGroupIndex mgi(0); mgi < scene_->NumMeshGroups(); ++mgi) {
    MeshGroup* const mg = scene_->GetMeshGroup(mgi);
    for (int mi = 0; mi < mg->NumMeshInstances(); ++mi) {
      MeshGroup::MeshInstance& mesh_instance = mg->GetMeshInstance(mi);
      if (mesh_instance.material_index == -1) {
        mesh_instance.material_index = default_material_index;
        default_material_needed = true;
      }
    }
  }
  if (default_material_needed) {
    // Create an empty default material (our defaults correspond to glTF
    // defaults).
    scene_->GetMaterialLibrary().MutableMaterial(default_material_index);
  }

  std::unordered_set<Mesh*> meshes_that_need_tangents;
  // Check if we need to generate tangent space for any of the loaded meshes.
  for (MeshGroupIndex mgi(0); mgi < scene_->NumMeshGroups(); ++mgi) {
    const MeshGroup* const mg = scene_->GetMeshGroup(mgi);
    for (int mi = 0; mi < mg->NumMeshInstances(); ++mi) {
      const MeshGroup::MeshInstance& mesh_instance = mg->GetMeshInstance(mi);
      const auto tangent_map =
          scene_->GetMaterialLibrary()
              .GetMaterial(mesh_instance.material_index)
              ->GetTextureMapByType(TextureMap::NORMAL_TANGENT_SPACE);
      if (tangent_map != nullptr) {
        Mesh& mesh = scene_->GetMesh(mesh_instance.mesh_index);
        if (mesh.GetNamedAttribute(GeometryAttribute::TANGENT) == nullptr) {
          meshes_that_need_tangents.insert(&mesh);
        }
      }
    }
  }

  return OkStatus();
}

Status GltfDecoder::AddSkinsToScene() {
  for (int source_skin_index = 0; source_skin_index < gltf_model_.skins.size();
       ++source_skin_index) {
    const tinygltf::Skin& skin = gltf_model_.skins[source_skin_index];
    const SkinIndex skin_index = scene_->AddSkin();
    Skin* const new_skin = scene_->GetSkin(skin_index);

    // The skin index was set previously while processing the nodes.
    if (skin_index.value() != source_skin_index) {
      return Status(Status::DRACO_ERROR, "Skin indices are mismatched.");
    }

    if (skin.inverseBindMatrices >= 0) {
      const tinygltf::Accessor& accessor =
          gltf_model_.accessors[skin.inverseBindMatrices];
      DRACO_RETURN_IF_ERROR(TinyGltfUtils::AddAccessorToAnimationData(
          gltf_model_, accessor, &new_skin->GetInverseBindMatrices()));
    }

    if (skin.skeleton >= 0) {
      const auto it = gltf_node_to_scenenode_index_.find(skin.skeleton);
      if (it == gltf_node_to_scenenode_index_.end()) {
        // TODO(b/200317162): If skeleton is not found set the default.
        return Status(Status::DRACO_ERROR,
                      "Could not find skeleton in the skin.");
      }
      new_skin->SetJointRoot(it->second);
    }

    for (int joint : skin.joints) {
      const auto it = gltf_node_to_scenenode_index_.find(joint);
      if (it == gltf_node_to_scenenode_index_.end()) {
        // TODO(b/200317162): If skeleton is not found set the default.
        return Status(Status::DRACO_ERROR,
                      "Could not find skeleton in the skin.");
      }
      new_skin->AddJoint(it->second);
    }
  }
  return OkStatus();
}

void GltfDecoder::MoveNonMaterialTextures(Mesh* mesh) {
  // Collect non-material textures in the |mesh| and move them from material
  // texture library to non-material texture library.
  std::unordered_set<Texture*> non_material_textures;
  CollectFeatureIdTextures(*mesh, non_material_textures);
  CollectPropertyTextures(mesh->GetStructuralMetadata(), non_material_textures);
  MoveNonMaterialTextures(non_material_textures,
                          &mesh->GetMaterialLibrary().MutableTextureLibrary(),
                          &mesh->GetNonMaterialTextureLibrary());
}

void GltfDecoder::MoveNonMaterialTextures(Scene* scene) {
  // Collect non-material textures in the |scene| and move them from material
  // texture library to non-material texture library.
  std::unordered_set<Texture*> non_material_textures;
  for (MeshIndex i(0); i < scene->NumMeshes(); i++) {
    CollectFeatureIdTextures(scene->GetMesh(i), non_material_textures);
  }
  CollectPropertyTextures(scene->GetStructuralMetadata(),
                          non_material_textures);

  // Move collected textures from material texture library to non-material
  // texture library.
  MoveNonMaterialTextures(non_material_textures,
                          &scene->GetMaterialLibrary().MutableTextureLibrary(),
                          &scene->GetNonMaterialTextureLibrary());
}

void GltfDecoder::CollectFeatureIdTextures(
    Mesh& mesh, std::unordered_set<Texture*>& textures) {
  for (MeshFeaturesIndex i(0); i < mesh.NumMeshFeatures(); i++) {
    Texture* const texture = mesh.GetMeshFeatures(i).GetTextureMap().texture();
    if (texture != nullptr) {
      textures.insert(texture);
    }
  }
}

void GltfDecoder::CollectPropertyTextures(
    StructuralMetadata& structural_metadata,
    std::unordered_set<Texture*>& textures) {
  for (int i = 0; i < structural_metadata.NumPropertyTextures(); i++) {
    PropertyTexture& property_texture =
        structural_metadata.GetPropertyTexture(i);
    for (int j = 0; j < property_texture.NumProperties(); j++) {
      Texture* const texture =
          property_texture.GetProperty(j).GetTextureMap().texture();
      textures.insert(texture);
    }
  }
}

void GltfDecoder::MoveNonMaterialTextures(
    const std::unordered_set<Texture*>& non_material_textures,
    TextureLibrary* material_tl, TextureLibrary* non_material_tl) {
  // TODO(vytyaz): Consider textures that are both material and non-material.
  for (int i = 0; i < material_tl->NumTextures(); i++) {
    // Move non-material texture from material to non-material texture library.
    if (non_material_textures.count(material_tl->GetTexture(i)) == 1) {
      non_material_tl->PushTexture(material_tl->RemoveTexture(i--));
    }
  }
}

bool GltfDecoder::PrimitiveSignature::operator==(
    const PrimitiveSignature& signature) const {
  return primitive.indices == signature.primitive.indices &&
         primitive.attributes == signature.primitive.attributes &&
         primitive.extras == signature.primitive.extras &&
         primitive.extensions == signature.primitive.extensions &&
         primitive.mode == signature.primitive.mode &&
         primitive.targets == signature.primitive.targets;
}

size_t GltfDecoder::PrimitiveSignature::Hash::operator()(
    const PrimitiveSignature& signature) const {
  size_t hash = 79;  // Magic number.
  hash = HashCombine(signature.primitive.attributes.size(), hash);
  for (auto it = signature.primitive.attributes.begin();
       it != signature.primitive.attributes.end(); ++it) {
    hash = HashCombine(it->first, hash);
    hash = HashCombine(it->second, hash);
  }
  hash = HashCombine(signature.primitive.indices, hash);
  hash = HashCombine(signature.primitive.mode, hash);
  return hash;
}

StatusOr<std::unique_ptr<Mesh>> GltfDecoder::BuildMeshFromBuilder(
    bool use_mesh_builder, TriangleSoupMeshBuilder* mb, PointCloudBuilder* pb,
    bool deduplicate_vertices) {
  std::unique_ptr<Mesh> mesh;
  if (use_mesh_builder) {
    mesh = mb->Finalize();
  } else {
    std::unique_ptr<PointCloud> pc = pb->Finalize(deduplicate_vertices);
    if (pc) {
      mesh.reset(new Mesh());
      PointCloud* mesh_pc = mesh.get();
      mesh_pc->Copy(*pc);
    }
  }
  if (!mesh) {
    return ErrorStatus("Failed to build Draco mesh from glTF data.");
  }
  return mesh;
}

Status GltfDecoder::AddAssetMetadata(Scene* scene) {
  return AddAssetMetadata(&scene->GetMetadata());
}

Status GltfDecoder::AddAssetMetadata(Mesh* mesh) {
  Metadata* metadata = nullptr;
  std::unique_ptr<GeometryMetadata> metadata_owned;
  // Use metadata of the mesh or create new one.
  if (mesh->GetMetadata() != nullptr) {
    metadata = mesh->metadata();
  } else {
    metadata_owned = std::make_unique<GeometryMetadata>();
    metadata = metadata_owned.get();
  }
  DRACO_RETURN_IF_ERROR(AddAssetMetadata(metadata));
  if (metadata_owned != nullptr && metadata->num_entries() > 0) {
    // Some metadata was added to the |metadata_owned| instance. Add it to the
    // mesh.
    mesh->AddMetadata(std::move(metadata_owned));
  }
  return OkStatus();
}

Status GltfDecoder::AddAssetMetadata(Metadata* metadata) {
  // Store the copyright information in the |metadata|.
  if (!gltf_model_.asset.copyright.empty()) {
    metadata->AddEntryString("copyright", gltf_model_.asset.copyright);
  }
  return OkStatus();
}

bool GltfDecoder::HasJoints(const tinygltf::Primitive& primitive) {
  return primitive.attributes.count("JOINTS_0") > 0;
}

}  // namespace draco

#endif  // DRACO_TRANSCODER_SUPPORTED
