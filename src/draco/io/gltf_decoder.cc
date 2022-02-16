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

#include <memory>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "draco/core/hash_utils.h"
#include "draco/core/status.h"
#include "draco/core/status_or.h"
#include "draco/io/tiny_gltf_utils.h"
#include "draco/mesh/mesh.h"
#include "draco/mesh/triangle_soup_mesh_builder.h"
#include "draco/scene/scene_indices.h"
#include "draco/texture/source_image.h"
#include "draco/texture/texture_utils.h"
#include "tiny_gltf.h"

namespace draco {

namespace {
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
  } else if (attribute_name == "TEXCOORD_0") {
    return GeometryAttribute::TEX_COORD;
  } else if (attribute_name == "TEXCOORD_1") {
    return GeometryAttribute::TEX_COORD;
  } else if (attribute_name == "TANGENT") {
    return GeometryAttribute::TANGENT;
  } else if (attribute_name == "COLOR_0") {
    return GeometryAttribute::COLOR;
  } else if (attribute_name == "JOINTS_0") {
    return GeometryAttribute::JOINTS;
  } else if (attribute_name == "WEIGHTS_0") {
    return GeometryAttribute::WEIGHTS;
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
    const tinygltf::Model &model, const tinygltf::Accessor &accessor) {
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
  if (accessor.bufferView < 0) {
    return Status(Status::DRACO_ERROR,
                  "Error CopyDataAsUint32() bufferView < 0.");
  }

  const tinygltf::BufferView &buffer_view =
      model.bufferViews[accessor.bufferView];
  if (buffer_view.buffer < 0) {
    return Status(Status::DRACO_ERROR, "Error CopyDataAsUint32() buffer < 0.");
  }

  const tinygltf::Buffer &buffer = model.buffers[buffer_view.buffer];

  const uint8_t *const data_start =
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
  const uint8_t *data = data_start;
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

template <typename VectorT>
StatusOr<std::vector<VectorT>> CopyDataAs(const tinygltf::Model &model,
                                          const tinygltf::Accessor &accessor) {
  const int num_components =
      TinyGltfUtils::GetNumComponentsForType(accessor.type);
  if (num_components != VectorT::dimension) {
    return Status(Status::DRACO_ERROR,
                  "Dimension does not equal num components.");
  }
  if (accessor.bufferView < 0) {
    return Status(Status::DRACO_ERROR, "Error CopyDataAs() bufferView < 0.");
  }

  const tinygltf::BufferView &buffer_view =
      model.bufferViews[accessor.bufferView];
  if (buffer_view.buffer < 0) {
    return Status(Status::DRACO_ERROR, "Error CopyDataAs() buffer < 0.");
  }

  const tinygltf::Buffer &buffer = model.buffers[buffer_view.buffer];

  const uint8_t *const data_start =
      buffer.data.data() + buffer_view.byteOffset + accessor.byteOffset;
  const int byte_stride = accessor.ByteStride(buffer_view);
  const int component_size =
      tinygltf::GetComponentSizeInBytes(accessor.componentType);

  std::vector<VectorT> output;
  output.resize(accessor.count);

  const uint8_t *data = data_start;
  for (int i = 0; i < accessor.count; ++i) {
    VectorT values;

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
Status CopyDataFromBufferView(const tinygltf::Model &model, int buffer_view_id,
                              std::vector<uint8_t> *data) {
  if (buffer_view_id < 0) {
    return Status(Status::DRACO_ERROR, "Error CopyDataAs() bufferView < 0.");
  }
  const tinygltf::BufferView &buffer_view = model.bufferViews[buffer_view_id];
  if (buffer_view.buffer < 0) {
    return Status(Status::DRACO_ERROR, "Error CopyDataAs() buffer < 0.");
  }
  if (buffer_view.byteStride != 0) {
    return Status(Status::DRACO_ERROR, "Error buffer view byteStride != 0.");
  }

  const tinygltf::Buffer &buffer = model.buffers[buffer_view.buffer];
  const uint8_t *const data_start = buffer.data.data() + buffer_view.byteOffset;

  data->resize(buffer_view.byteLength);
  memcpy(&(*data)[0], data_start, buffer_view.byteLength);
  return OkStatus();
}

// Returns a SourceImage created from |image|.
StatusOr<std::unique_ptr<SourceImage>> GetSourceImage(
    const tinygltf::Model &model, const tinygltf::Image &image,
    const Texture &texture) {
  std::unique_ptr<SourceImage> source_image(new SourceImage());
  // If the image is in an external file then the buffer view is < 0.
  if (image.bufferView >= 0) {
    DRACO_RETURN_IF_ERROR(CopyDataFromBufferView(
        model, image.bufferView, &source_image->MutableEncodedData()));
  }
  source_image->set_filename(image.uri);
  source_image->set_mime_type(image.mimeType);

  return source_image;
}

std::unique_ptr<TrsMatrix> GetNodeTrsMatrix(const tinygltf::Node &node) {
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
    const Eigen::Matrix4d &transform_matrix) {
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

float Determinant(const Eigen::Matrix4d &transform_matrix) {
  Eigen::Matrix3d mat3x3;
  // clang-format off
  mat3x3 <<
      transform_matrix(0, 0), transform_matrix(0, 1), transform_matrix(0, 2),
      transform_matrix(1, 0), transform_matrix(1, 1), transform_matrix(1, 2),
      transform_matrix(2, 0), transform_matrix(2, 1), transform_matrix(2, 2);
  // clang-format on
  return mat3x3.determinant();
}

bool FileExists(const std::string &filepath, void * /*user_data*/) {
  return GetFileSize(filepath) != 0;
}

bool ReadWholeFile(std::vector<unsigned char> *out, std::string *err,
                   const std::string &filepath, void *user_data) {
  if (!ReadFileToBuffer(filepath, out)) {
    if (err) {
      *err = "Unable to read: " + filepath;
    }
    return false;
  }
  if (user_data) {
    auto *files_vector =
        reinterpret_cast<std::vector<std::string> *>(user_data);
    files_vector->push_back(filepath);
  }
  return true;
}

bool WriteWholeFile(std::string * /*err*/, const std::string &filepath,
                    const std::vector<unsigned char> &contents,
                    void * /*user_data*/) {
  return WriteBufferToFile(contents.data(), contents.size(), filepath);
}

}  // namespace

GltfDecoder::GltfDecoder()
    : next_face_id_(0), total_indices_count_(0), material_att_id_(-1) {}

StatusOr<std::unique_ptr<Mesh>> GltfDecoder::DecodeFromFile(
    const std::string &file_name) {
  return DecodeFromFile(file_name, nullptr);
}

StatusOr<std::unique_ptr<Mesh>> GltfDecoder::DecodeFromFile(
    const std::string &file_name, std::vector<std::string> *mesh_files) {
  DRACO_RETURN_IF_ERROR(LoadFile(file_name, mesh_files));
  return BuildMesh();
}

StatusOr<std::unique_ptr<Mesh>> GltfDecoder::DecodeFromBuffer(
    DecoderBuffer *buffer) {
  DRACO_RETURN_IF_ERROR(LoadBuffer(*buffer));
  return BuildMesh();
}

StatusOr<std::unique_ptr<Scene>> GltfDecoder::DecodeFromFileToScene(
    const std::string &file_name) {
  return DecodeFromFileToScene(file_name, nullptr);
}

StatusOr<std::unique_ptr<Scene>> GltfDecoder::DecodeFromFileToScene(
    const std::string &file_name, std::vector<std::string> *scene_files) {
  DRACO_RETURN_IF_ERROR(LoadFile(file_name, scene_files));
  scene_ = std::unique_ptr<Scene>(new Scene());
  DRACO_RETURN_IF_ERROR(DecodeGltfToScene());
  return std::move(scene_);
}

StatusOr<std::unique_ptr<Scene>> GltfDecoder::DecodeFromBufferToScene(
    DecoderBuffer *buffer) {
  DRACO_RETURN_IF_ERROR(LoadBuffer(*buffer));
  scene_ = std::unique_ptr<Scene>(new Scene());
  DRACO_RETURN_IF_ERROR(DecodeGltfToScene());
  return std::move(scene_);
}

Status GltfDecoder::LoadFile(const std::string &file_name,
                             std::vector<std::string> *input_files) {
  const std::string extension = LowercaseFileExtension(file_name);
  tinygltf::TinyGLTF loader;
  std::string err;
  std::string warn;

  const tinygltf::FsCallbacks fs_callbacks = {
      &FileExists,
      // TinyGLTF's ExpandFilePath does not do filesystem i/o, so it's safe to
      // use in all environments.
      &tinygltf::ExpandFilePath, &ReadWholeFile, &WriteWholeFile,
      reinterpret_cast<void *>(input_files)};

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

Status GltfDecoder::LoadBuffer(const DecoderBuffer &buffer) {
  tinygltf::TinyGLTF loader;
  std::string err;
  std::string warn;
  if (!loader.LoadBinaryFromMemory(
          &gltf_model_, &err, &warn,
          reinterpret_cast<const unsigned char *>(buffer.data_head()),
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
  mb_.Start(total_indices_count_ / 3);
  DRACO_RETURN_IF_ERROR(AddAttributesToDracoMesh());

  for (const tinygltf::Scene &scene : gltf_model_.scenes) {
    for (int i = 0; i < scene.nodes.size(); ++i) {
      const Eigen::Matrix4d parent_matrix = Eigen::Matrix4d::Identity();
      DRACO_RETURN_IF_ERROR(DecodeNode(scene.nodes[i], parent_matrix));
    }
  }
  std::unique_ptr<Mesh> mesh = mb_.Finalize();

  DRACO_RETURN_IF_ERROR(CopyTextures<Mesh>(mesh.get()));
  DRACO_RETURN_IF_ERROR(AddMaterialsToDracoMesh(mesh.get()));
  return mesh;
}

Status GltfDecoder::CheckUnsupportedFeatures() {
  // Check for morph targets.
  for (const auto &mesh : gltf_model_.meshes) {
    for (const auto &primitive : mesh.primitives) {
      if (!primitive.targets.empty()) {
        return Status(Status::UNSUPPORTED_FEATURE,
                      "Morph targets are unsupported.");
      }
    }
  }

  // Check for sparse accessors.
  for (const auto &accessor : gltf_model_.accessors) {
    if (accessor.sparse.isSparse) {
      return Status(Status::UNSUPPORTED_FEATURE,
                    "Sparse accessors are unsupported.");
    }
  }

  // Check for extensions.
  for (const auto &extension : gltf_model_.extensionsRequired) {
    if (extension != "KHR_materials_unlit" &&
        extension != "KHR_texture_transform" &&
        extension != "KHR_draco_mesh_compression") {
      return Status(Status::UNSUPPORTED_FEATURE,
                    extension + " is unsupported.");
    }
  }
  return OkStatus();
}

Status GltfDecoder::DecodeNode(int node_index,
                               const Eigen::Matrix4d &parent_matrix) {
  const tinygltf::Node &node = gltf_model_.nodes[node_index];
  const std::unique_ptr<TrsMatrix> trsm = GetNodeTrsMatrix(node);
  const Eigen::Matrix4d node_matrix =
      parent_matrix * trsm->ComputeTransformationMatrix();

  if (node.mesh >= 0) {
    const tinygltf::Mesh &mesh = gltf_model_.meshes[node.mesh];
    for (const auto &primitive : mesh.primitives) {
      DRACO_RETURN_IF_ERROR(DecodePrimitive(primitive, node_matrix));
    }
  }
  for (int i = 0; i < node.children.size(); ++i) {
    DRACO_RETURN_IF_ERROR(DecodeNode(node.children[i], node_matrix));
  }
  return OkStatus();
}

StatusOr<int> GltfDecoder::DecodePrimitiveAttributeCount(
    const tinygltf::Primitive &primitive) const {
  // Use the first primitive attribute as all attributes have the same entry
  // count according to glTF 2.0 spec.
  if (primitive.attributes.empty()) {
    return Status(Status::DRACO_ERROR, "Primitive has no attributes.");
  }
  const tinygltf::Accessor &accessor =
      gltf_model_.accessors[primitive.attributes.begin()->second];
  return accessor.count;
}

StatusOr<int> GltfDecoder::DecodePrimitiveIndicesCount(
    const tinygltf::Primitive &primitive) const {
  if (primitive.indices < 0) {
    // Primitive has implicit indices [0, 1, 2, 3, ...]. Determine indices count
    // based on entry count of a primitive attribute.
    return DecodePrimitiveAttributeCount(primitive);
  }
  const tinygltf::Accessor &indices = gltf_model_.accessors[primitive.indices];
  return indices.count;
}

StatusOr<std::vector<uint32_t>> GltfDecoder::DecodePrimitiveIndices(
    const tinygltf::Primitive &primitive) const {
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
    const tinygltf::Accessor &indices =
        gltf_model_.accessors[primitive.indices];
    if (indices.count <= 0) {
      return Status(Status::DRACO_ERROR, "Could not convert indices.");
    }
    DRACO_ASSIGN_OR_RETURN(indices_data,
                           CopyDataAsUint32(gltf_model_, indices));
  }
  return indices_data;
}

Status GltfDecoder::DecodePrimitive(const tinygltf::Primitive &primitive,
                                    const Eigen::Matrix4d &transform_matrix) {
  if (primitive.mode != TINYGLTF_MODE_TRIANGLES) {
    return Status(Status::DRACO_ERROR, "Primitive does not contain triangles.");
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

  for (const auto &attribute : primitive.attributes) {
    const tinygltf::Accessor &accessor =
        gltf_model_.accessors[attribute.second];

    const int att_id =
        attribute_name_to_draco_mesh_attribute_id_[attribute.first];
    if (att_id == -1) {
      continue;
    }

    const bool reverse_winding = Determinant(transform_matrix) < 0;
    if (attribute.first == "TEXCOORD_0" || attribute.first == "TEXCOORD_1") {
      DRACO_RETURN_IF_ERROR(AddTexCoordToMeshBuilder(accessor, indices_data,
                                                     att_id, number_of_faces,
                                                     reverse_winding, &mb_));
    } else if (attribute.first == "TANGENT") {
      const Eigen::Matrix4d matrix = UpdateMatrixForNormals(transform_matrix);
      DRACO_RETURN_IF_ERROR(AddTangentToMeshBuilder(
          accessor, indices_data, att_id, number_of_faces, matrix,
          reverse_winding, &mb_));
    } else if (attribute.first == "POSITION" || attribute.first == "NORMAL") {
      const Eigen::Matrix4d matrix =
          (attribute.first == "NORMAL")
              ? UpdateMatrixForNormals(transform_matrix)
              : transform_matrix;
      const bool normalize = (attribute.first == "NORMAL");
      DRACO_RETURN_IF_ERROR(AddTransformedDataToMeshBuilder(
          accessor, indices_data, att_id, number_of_faces, matrix, normalize,
          reverse_winding, &mb_));
    } else {
      DRACO_RETURN_IF_ERROR(AddAttributeDataByTypes(accessor, indices_data,
                                                    att_id, number_of_faces,
                                                    reverse_winding, &mb_));
    }
  }

  // Add the material data only if there is more than one material.
  if (gltf_primitive_material_to_draco_material_.size() > 1) {
    const int material_index = primitive.material;
    const auto it =
        gltf_primitive_material_to_draco_material_.find(material_index);
    if (it != gltf_primitive_material_to_draco_material_.end()) {
      if (gltf_primitive_material_to_draco_material_.size() < 256) {
        const uint8_t material_value = it->second;
        DRACO_RETURN_IF_ERROR(AddMaterialDataToMeshBuilder<uint8_t>(
            material_value, number_of_faces));
      } else if (gltf_primitive_material_to_draco_material_.size() <
                 (1 << 16)) {
        const uint16_t material_value = it->second;
        DRACO_RETURN_IF_ERROR(AddMaterialDataToMeshBuilder<uint16_t>(
            material_value, number_of_faces));
      } else {
        const uint32_t material_value = it->second;
        DRACO_RETURN_IF_ERROR(AddMaterialDataToMeshBuilder<uint32_t>(
            material_value, number_of_faces));
      }
    }
  }

  next_face_id_ += number_of_faces;
  return OkStatus();
}

Status GltfDecoder::NodeGatherAttributeAndMaterialStats(
    const tinygltf::Node &node) {
  if (node.mesh >= 0) {
    const tinygltf::Mesh &mesh = gltf_model_.meshes[node.mesh];
    for (const auto &primitive : mesh.primitives) {
      DRACO_RETURN_IF_ERROR(AccumulatePrimitiveStats(primitive));

      const auto it =
          gltf_primitive_material_to_draco_material_.find(primitive.material);
      if (it == gltf_primitive_material_to_draco_material_.end()) {
        gltf_primitive_material_to_draco_material_[primitive.material] =
            gltf_primitive_material_to_draco_material_.size();
      }
    }
  }
  for (int i = 0; i < node.children.size(); ++i) {
    const tinygltf::Node &child = gltf_model_.nodes[node.children[i]];
    DRACO_RETURN_IF_ERROR(NodeGatherAttributeAndMaterialStats(child));
  }

  return OkStatus();
}

Status GltfDecoder::GatherAttributeAndMaterialStats() {
  for (const auto &scene : gltf_model_.scenes) {
    for (int i = 0; i < scene.nodes.size(); ++i) {
      const tinygltf::Node &node = gltf_model_.nodes[scene.nodes[i]];
      DRACO_RETURN_IF_ERROR(NodeGatherAttributeAndMaterialStats(node));
    }
  }
  return OkStatus();
}

void GltfDecoder::SumAttributeStats(const std::string &attribute_name,
                                    int count) {
  const auto it = total_attribute_counts_.find(attribute_name);
  if (it == total_attribute_counts_.end()) {
    total_attribute_counts_[attribute_name] = count;
  } else {
    total_attribute_counts_[attribute_name] += count;
  }
}

Status GltfDecoder::CheckTypes(const std::string &attribute_name,
                               int component_type, int type) {
  const auto it_ct = attribute_component_type_.find(attribute_name);
  if (it_ct == attribute_component_type_.end()) {
    attribute_component_type_[attribute_name] = component_type;
  } else if (attribute_component_type_[attribute_name] != component_type) {
    return Status(
        Status::DRACO_ERROR,
        attribute_name + " attribute component type does not match previous.");
  }

  const auto it_t = attribute_type_.find(attribute_name);
  if (it_t == attribute_type_.end()) {
    attribute_type_[attribute_name] = type;
  } else if (attribute_type_[attribute_name] != type) {
    return Status(Status::DRACO_ERROR,
                  attribute_name + " attribute type does not match previous.");
  }
  return OkStatus();
}

Status GltfDecoder::AccumulatePrimitiveStats(
    const tinygltf::Primitive &primitive) {
  DRACO_ASSIGN_OR_RETURN(const int indices_count,
                         DecodePrimitiveIndicesCount(primitive));
  total_indices_count_ += indices_count;

  for (const auto &attribute : primitive.attributes) {
    const tinygltf::Accessor &accessor =
        gltf_model_.accessors[attribute.second];

    DRACO_RETURN_IF_ERROR(
        CheckTypes(attribute.first, accessor.componentType, accessor.type));
    SumAttributeStats(attribute.first, accessor.count);
  }
  return OkStatus();
}

Status GltfDecoder::AddAttributesToDracoMesh() {
  for (const auto &attribute : total_attribute_counts_) {
    const GeometryAttribute::Type draco_att_type =
        GltfAttributeToDracoAttribute(attribute.first);
    if (draco_att_type == GeometryAttribute::INVALID) {
      // Map an invalid attribute to attribute id -1 that will be ignored and
      // not included in the Draco mesh.
      attribute_name_to_draco_mesh_attribute_id_[attribute.first] = -1;
      continue;
    }
    DRACO_ASSIGN_OR_RETURN(
        const int att_id,
        AddAttribute(draco_att_type, attribute_component_type_[attribute.first],
                     attribute_type_[attribute.first], &mb_));
    attribute_name_to_draco_mesh_attribute_id_[attribute.first] = att_id;
  }

  // Add the material attribute.
  if (gltf_model_.materials.size() > 1) {
    draco::DataType component_type = DT_UINT32;
    if (gltf_model_.materials.size() < 256) {
      component_type = DT_UINT8;
    } else if (gltf_model_.materials.size() < (1 << 16)) {
      component_type = DT_UINT16;
    }
    material_att_id_ =
        mb_.AddAttribute(GeometryAttribute::MATERIAL, 1, component_type);
  }

  return OkStatus();
}

Status GltfDecoder::AddTangentToMeshBuilder(
    const tinygltf::Accessor &accessor,
    const std::vector<uint32_t> &indices_data, int att_id, int number_of_faces,
    const Eigen::Matrix4d &transform_matrix, bool reverse_winding,
    TriangleSoupMeshBuilder *mb) {
  DRACO_ASSIGN_OR_RETURN(
      std::vector<Vector4f> data,
      TinyGltfUtils::CopyDataAsFloat<Vector4f>(gltf_model_, accessor));

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

  SetValuesPerFace<Vector4f>(indices_data, att_id, number_of_faces, data,
                             reverse_winding, mb);
  return OkStatus();
}

Status GltfDecoder::AddTexCoordToMeshBuilder(
    const tinygltf::Accessor &accessor,
    const std::vector<uint32_t> &indices_data, int att_id, int number_of_faces,
    bool reverse_winding, TriangleSoupMeshBuilder *mb) {
  DRACO_ASSIGN_OR_RETURN(
      std::vector<Vector2f> data,
      TinyGltfUtils::CopyDataAsFloat<Vector2f>(gltf_model_, accessor));

  // glTF stores texture coordinates flipped on the horizontal axis compared to
  // how Draco stores texture coordinates.
  for (auto &uv : data) {
    uv[1] = 1.0 - uv[1];
  }

  SetValuesPerFace<Vector2f>(indices_data, att_id, number_of_faces, data,
                             reverse_winding, mb);
  return OkStatus();
}

Status GltfDecoder::AddTransformedDataToMeshBuilder(
    const tinygltf::Accessor &accessor,
    const std::vector<uint32_t> &indices_data, int att_id, int number_of_faces,
    const Eigen::Matrix4d &transform_matrix, bool normalize,
    bool reverse_winding, TriangleSoupMeshBuilder *mb) {
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

  SetValuesPerFace<Vector3f>(indices_data, att_id, number_of_faces, data,
                             reverse_winding, mb);
  return OkStatus();
}

template <typename T>
void GltfDecoder::SetValuesPerFace(const std::vector<uint32_t> &indices_data,
                                   int att_id, int number_of_faces,
                                   const std::vector<T> &data,
                                   bool reverse_winding,
                                   TriangleSoupMeshBuilder *mb) {
  for (int f = 0; f < number_of_faces; ++f) {
    const int base_corner = f * 3;
    const uint32_t v_id = indices_data[base_corner];
    const int next_offset = reverse_winding ? 2 : 1;
    const int prev_offset = reverse_winding ? 1 : 2;
    const uint32_t v_next_id = indices_data[base_corner + next_offset];
    const uint32_t v_prev_id = indices_data[base_corner + prev_offset];

    const FaceIndex face_index(f + next_face_id_);
    mb->SetAttributeValuesForFace(att_id, face_index, data[v_id].data(),
                                  data[v_next_id].data(),
                                  data[v_prev_id].data());
  }
}

Status GltfDecoder::AddAttributeDataByTypes(
    const tinygltf::Accessor &accessor,
    const std::vector<uint32_t> &indices_data, int att_id, int number_of_faces,
    bool reverse_winding, TriangleSoupMeshBuilder *mb) {
  typedef VectorD<uint8_t, 2> Vector2u8i;
  typedef VectorD<uint8_t, 3> Vector3u8i;
  typedef VectorD<uint8_t, 4> Vector4u8i;
  typedef VectorD<uint16_t, 2> Vector2u16i;
  typedef VectorD<uint16_t, 3> Vector3u16i;
  typedef VectorD<uint16_t, 4> Vector4u16i;
  switch (accessor.type) {
    case TINYGLTF_TYPE_VEC2:
      switch (accessor.componentType) {
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
          DRACO_ASSIGN_OR_RETURN(std::vector<Vector2u8i> data,
                                 CopyDataAs<Vector2u8i>(gltf_model_, accessor));
          SetValuesPerFace<Vector2u8i>(indices_data, att_id, number_of_faces,
                                       data, reverse_winding, mb);
        } break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
          DRACO_ASSIGN_OR_RETURN(
              std::vector<Vector2u16i> data,
              CopyDataAs<Vector2u16i>(gltf_model_, accessor));
          SetValuesPerFace<Vector2u16i>(indices_data, att_id, number_of_faces,
                                        data, reverse_winding, mb);
        } break;
        case TINYGLTF_COMPONENT_TYPE_FLOAT: {
          DRACO_ASSIGN_OR_RETURN(
              std::vector<Vector2f> data,
              TinyGltfUtils::CopyDataAsFloat<Vector2f>(gltf_model_, accessor));
          SetValuesPerFace<Vector2f>(indices_data, att_id, number_of_faces,
                                     data, reverse_winding, mb);
        } break;
        default:
          return Status(Status::DRACO_ERROR,
                        "Add attribute data, unknown component type.");
      }
      break;
    case TINYGLTF_TYPE_VEC3:
      switch (accessor.componentType) {
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
          DRACO_ASSIGN_OR_RETURN(std::vector<Vector3u8i> data,
                                 CopyDataAs<Vector3u8i>(gltf_model_, accessor));
          SetValuesPerFace<Vector3u8i>(indices_data, att_id, number_of_faces,
                                       data, reverse_winding, mb);
        } break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
          DRACO_ASSIGN_OR_RETURN(
              std::vector<Vector3u16i> data,
              CopyDataAs<Vector3u16i>(gltf_model_, accessor));
          SetValuesPerFace<Vector3u16i>(indices_data, att_id, number_of_faces,
                                        data, reverse_winding, mb);
        } break;
        case TINYGLTF_COMPONENT_TYPE_FLOAT: {
          DRACO_ASSIGN_OR_RETURN(
              std::vector<Vector3f> data,
              TinyGltfUtils::CopyDataAsFloat<Vector3f>(gltf_model_, accessor));
          SetValuesPerFace<Vector3f>(indices_data, att_id, number_of_faces,
                                     data, reverse_winding, mb);
        } break;
        default:
          return Status(Status::DRACO_ERROR,
                        "Add attribute data, unknown component type.");
      }
      break;
    case TINYGLTF_TYPE_VEC4:
      switch (accessor.componentType) {
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
          DRACO_ASSIGN_OR_RETURN(std::vector<Vector4u8i> data,
                                 CopyDataAs<Vector4u8i>(gltf_model_, accessor));
          SetValuesPerFace<Vector4u8i>(indices_data, att_id, number_of_faces,
                                       data, reverse_winding, mb);
        } break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
          DRACO_ASSIGN_OR_RETURN(
              std::vector<Vector4u16i> data,
              CopyDataAs<Vector4u16i>(gltf_model_, accessor));
          SetValuesPerFace<Vector4u16i>(indices_data, att_id, number_of_faces,
                                        data, reverse_winding, mb);
        } break;
        case TINYGLTF_COMPONENT_TYPE_FLOAT: {
          DRACO_ASSIGN_OR_RETURN(
              std::vector<Vector4f> data,
              TinyGltfUtils::CopyDataAsFloat<Vector4f>(gltf_model_, accessor));
          SetValuesPerFace<Vector4f>(indices_data, att_id, number_of_faces,
                                     data, reverse_winding, mb);
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
Status GltfDecoder::CopyTextures(T *owner) {
  for (int i = 0; i < gltf_model_.images.size(); ++i) {
    const tinygltf::Image &image = gltf_model_.images[i];
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

Status GltfDecoder::AddMaterialsToDracoMesh(Mesh *mesh) {
  bool is_normal_map_used = false;

  int default_material_index = -1;
  const auto it = gltf_primitive_material_to_draco_material_.find(-1);
  if (it != gltf_primitive_material_to_draco_material_.end()) {
    default_material_index = it->second;
  }

  int output_material_index = 0;
  for (int input_material_index = 0;
       input_material_index < gltf_model_.materials.size();
       ++input_material_index) {
    if (default_material_index == input_material_index) {
      // Insert a default material here for primitives that did not have a
      // material index.
      mesh->GetMaterialLibrary().MutableMaterial(output_material_index++);
    }

    Material *const output_material =
        mesh->GetMaterialLibrary().MutableMaterial(output_material_index++);
    DRACO_RETURN_IF_ERROR(
        AddGltfMaterial(input_material_index, output_material));
    if (output_material->GetTextureMapByType(
            TextureMap::NORMAL_TANGENT_SPACE)) {
      is_normal_map_used = true;
    }
  }

  return OkStatus();
}

template <typename T>
Status GltfDecoder::AddMaterialDataToMeshBuilder(T material_value,
                                                 int number_of_faces) {
  for (int f = 0; f < number_of_faces; ++f) {
    const FaceIndex face_index(f + next_face_id_);
    mb_.SetPerFaceAttributeValueForFace(material_att_id_, face_index,
                                        &material_value);
  }
  return OkStatus();
}

Status GltfDecoder::CheckAndAddTextureToDracoMaterial(
    int texture_index, int tex_coord_attribute_index,
    const tinygltf::ExtensionMap &tex_info_ext, Material *material,
    TextureMap::Type type) {
  if (texture_index < 0) {
    return OkStatus();
  }

  const tinygltf::Texture &input_texture = gltf_model_.textures[texture_index];
  const auto texture_it =
      gltf_image_to_draco_texture_.find(input_texture.source);
  if (texture_it != gltf_image_to_draco_texture_.end()) {
    Texture *const texture = texture_it->second;
    // Default GLTF 2.0 sampler uses REPEAT mode along both S and T directions.
    TextureMap::WrappingMode wrapping_mode(TextureMap::REPEAT);
    TextureMap::FilterType min_filter = TextureMap::UNSPECIFIED;
    TextureMap::FilterType mag_filter = TextureMap::UNSPECIFIED;

    if (input_texture.sampler >= 0) {
      const tinygltf::Sampler &sampler =
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
      DRACO_RETURN_IF_ERROR(
          material->SetTextureMap(texture, type, wrapping_mode, min_filter,
                                  mag_filter, tex_coord_attribute_index));
    }
  }
  return OkStatus();
}

Status GltfDecoder::DecodeGltfToScene() {
  DRACO_RETURN_IF_ERROR(GatherAttributeAndMaterialStats());
  DRACO_RETURN_IF_ERROR(AddLightsToScene());
  for (const tinygltf::Scene &scene : gltf_model_.scenes) {
    for (int i = 0; i < scene.nodes.size(); ++i) {
      DRACO_RETURN_IF_ERROR(
          DecodeNodeForScene(scene.nodes[i], kInvalidSceneNodeIndex));
      scene_->AddRootNodeIndex(gltf_node_to_scenenode_index_[scene.nodes[i]]);
    }
  }

  DRACO_RETURN_IF_ERROR(AddAnimationsToScene());
  DRACO_RETURN_IF_ERROR(CopyTextures<Scene>(scene_.get()));
  DRACO_RETURN_IF_ERROR(AddMaterialsToScene());
  DRACO_RETURN_IF_ERROR(AddSkinsToScene());

  return OkStatus();
}

Status GltfDecoder::AddLightsToScene() {
  // Add all lights to Draco scene.
  for (const auto &light : gltf_model_.lights) {
    // Add a new light to the scene.
    const LightIndex light_index = scene_->AddLight();
    Light *scene_light = scene_->GetLight(light_index);

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

Status GltfDecoder::AddAnimationsToScene() {
  for (const auto &animation : gltf_model_.animations) {
    const AnimationIndex animation_index = scene_->AddAnimation();
    Animation *const encoder_animation = scene_->GetAnimation(animation_index);
    encoder_animation->SetName(animation.name);

    for (const tinygltf::AnimationChannel &channel : animation.channels) {
      const auto it = gltf_node_to_scenenode_index_.find(channel.target_node);
      if (it == gltf_node_to_scenenode_index_.end()) {
        return Status(Status::DRACO_ERROR, "Could not find Node in the scene.");
      }
      DRACO_RETURN_IF_ERROR(TinyGltfUtils::AddChannelToAnimation(
          gltf_model_, animation, channel, it->second.value(),
          encoder_animation));
    }
  }
  return OkStatus();
}

Status GltfDecoder::DecodeNodeForScene(int node_index,
                                       SceneNodeIndex parent_index) {
  SceneNodeIndex scene_node_index = kInvalidSceneNodeIndex;
  SceneNode *scene_node = nullptr;
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
    SceneNode *const parent_node = scene_->GetNode(parent_index);
    parent_node->AddChildIndex(scene_node_index);
  }

  if (!is_new_node) {
    return OkStatus();
  }
  const tinygltf::Node &node = gltf_model_.nodes[node_index];
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
    // Check if we have already parsed this glTF Mesh.
    const auto it = gltf_mesh_to_scene_mesh_group_.find(node.mesh);
    if (it != gltf_mesh_to_scene_mesh_group_.end()) {
      // We already processed this glTF mesh.
      scene_node->SetMeshGroupIndex(it->second);
    } else {
      const MeshGroupIndex scene_mesh_group_index = scene_->AddMeshGroup();
      MeshGroup *const scene_mesh =
          scene_->GetMeshGroup(scene_mesh_group_index);

      const tinygltf::Mesh &mesh = gltf_model_.meshes[node.mesh];
      if (!mesh.name.empty()) {
        scene_mesh->SetName(mesh.name);
      }
      for (const auto &primitive : mesh.primitives) {
        DRACO_RETURN_IF_ERROR(DecodePrimitiveForScene(primitive, scene_mesh));
      }
      scene_node->SetMeshGroupIndex(scene_mesh_group_index);
      gltf_mesh_to_scene_mesh_group_[node.mesh] = scene_mesh_group_index;
    }
  }

  // Decode light index.
  const auto &e = node.extensions.find("KHR_lights_punctual");
  if (e != node.extensions.end()) {
    const tinygltf::Value::Object &o = e->second.Get<tinygltf::Value::Object>();
    const auto &light = o.find("light");
    if (light != o.end()) {
      const tinygltf::Value &value = light->second;
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
    const tinygltf::Primitive &primitive, MeshGroup *mesh_group) {
  if (primitive.mode != TINYGLTF_MODE_TRIANGLES) {
    return Status(Status::DRACO_ERROR, "Primitive does not contain triangles.");
  }

  const PrimitiveSignature signature(primitive);
  const auto exisitng_mesh_index =
      gltf_primitive_to_draco_mesh_index_.find(signature);
  if (exisitng_mesh_index != gltf_primitive_to_draco_mesh_index_.end()) {
    mesh_group->AddMeshIndex(exisitng_mesh_index->second);
    mesh_group->AddMaterialIndex(primitive.material);
    return OkStatus();
  }

  // Handle indices first.
  DRACO_ASSIGN_OR_RETURN(const std::vector<uint32_t> indices_data,
                         DecodePrimitiveIndices(primitive));
  const int number_of_faces = indices_data.size() / 3;

  // Note that glTF mesh |primitive| has no name; no name is set to Draco mesh.
  TriangleSoupMeshBuilder mb;
  mb.Start(number_of_faces);

  for (const auto &attribute : primitive.attributes) {
    const tinygltf::Accessor &accessor =
        gltf_model_.accessors[attribute.second];
    const int component_type = accessor.componentType;
    const int type = accessor.type;
    DRACO_ASSIGN_OR_RETURN(
        const int att_id,
        AddAttribute(attribute.first, component_type, type, &mb));
    if (att_id == -1) {
      continue;
    }

    const bool reverse_winding = false;
    if (attribute.first == "TEXCOORD_0" || attribute.first == "TEXCOORD_1") {
      DRACO_RETURN_IF_ERROR(AddTexCoordToMeshBuilder(accessor, indices_data,
                                                     att_id, number_of_faces,
                                                     reverse_winding, &mb));
    } else if (attribute.first == "TANGENT") {
      const Eigen::Matrix4d matrix = Eigen::Matrix4d::Identity();
      DRACO_RETURN_IF_ERROR(AddTangentToMeshBuilder(
          accessor, indices_data, att_id, number_of_faces, matrix,
          reverse_winding, &mb));
    } else if (attribute.first == "POSITION" || attribute.first == "NORMAL") {
      const Eigen::Matrix4d matrix = Eigen::Matrix4d::Identity();
      const bool normalize = (attribute.first == "NORMAL");
      DRACO_RETURN_IF_ERROR(AddTransformedDataToMeshBuilder(
          accessor, indices_data, att_id, number_of_faces, matrix, normalize,
          reverse_winding, &mb));
    } else {
      DRACO_RETURN_IF_ERROR(AddAttributeDataByTypes(accessor, indices_data,
                                                    att_id, number_of_faces,
                                                    reverse_winding, &mb));
    }
  }

  int material_index = primitive.material;

  std::unique_ptr<Mesh> mesh = mb.Finalize();
  if (mesh == nullptr) {
    return Status(Status::DRACO_ERROR, "Could not build Draco mesh.");
  }
  const MeshIndex mesh_index = scene_->AddMesh(std::move(mesh));
  if (mesh_index == kInvalidMeshIndex) {
    return Status(Status::DRACO_ERROR, "Could not add Draco mesh to scene.");
  }
  mesh_group->AddMeshIndex(mesh_index);
  mesh_group->AddMaterialIndex(material_index);

  gltf_primitive_to_draco_mesh_index_[signature] = mesh_index;
  return OkStatus();
}

StatusOr<int> GltfDecoder::AddAttribute(const std::string &attribute_name,
                                        int component_type, int type,
                                        TriangleSoupMeshBuilder *mb) {
  const GeometryAttribute::Type draco_att_type =
      GltfAttributeToDracoAttribute(attribute_name);
  if (draco_att_type == GeometryAttribute::INVALID) {
    return Status(Status::DRACO_ERROR,
                  "Attribute " + attribute_name + " is not supported.");
  }
  DRACO_ASSIGN_OR_RETURN(
      const int att_id, AddAttribute(draco_att_type, component_type, type, mb));
  return att_id;
}

StatusOr<int> GltfDecoder::AddAttribute(GeometryAttribute::Type attribute_type,
                                        int component_type, int type,
                                        TriangleSoupMeshBuilder *mb) {
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
  const int att_id =
      mb->AddAttribute(attribute_type, num_components, draco_component_type);
  if (att_id < 0) {
    return Status(Status::DRACO_ERROR, "Could not add attribute.");
  }
  return att_id;
}

StatusOr<bool> GltfDecoder::CheckKhrTextureTransform(
    const tinygltf::ExtensionMap &extension, TextureTransform *transform) {
  bool transform_set = false;

  const auto &e = extension.find("KHR_texture_transform");
  if (e == extension.end()) {
    return false;
  }
  const tinygltf::Value::Object &o = e->second.Get<tinygltf::Value::Object>();
  const auto &scale = o.find("scale");
  if (scale != o.end()) {
    const tinygltf::Value &array = scale->second;
    if (!array.IsArray() || array.Size() != 2) {
      return Status(Status::DRACO_ERROR,
                    "KhrTextureTransform scale is malformed.");
    }
    std::array<double, 2> scale;
    for (int i = 0; i < array.Size(); i++) {
      const tinygltf::Value &value = array.Get(i);
      if (!value.IsNumber()) {
        return Status(Status::DRACO_ERROR,
                      "KhrTextureTransform scale is malformed.");
      }
      scale[i] = value.Get<double>();
      transform_set = true;
    }
    transform->set_scale(scale);
  }
  const auto &rotation = o.find("rotation");
  if (rotation != o.end()) {
    const tinygltf::Value &value = rotation->second;
    if (!value.IsNumber()) {
      return Status(Status::DRACO_ERROR,
                    "KhrTextureTransform rotation is malformed.");
    }
    transform->set_rotation(value.Get<double>());
    transform_set = true;
  }
  const auto &offset = o.find("offset");
  if (offset != o.end()) {
    const tinygltf::Value &array = offset->second;
    if (!array.IsArray() || array.Size() != 2) {
      return Status(Status::DRACO_ERROR,
                    "KhrTextureTransform offset is malformed.");
    }
    std::array<double, 2> offset;
    for (int i = 0; i < array.Size(); i++) {
      const tinygltf::Value &value = array.Get(i);
      if (!value.IsNumber()) {
        return Status(Status::DRACO_ERROR,
                      "KhrTextureTransform offset is malformed.");
      }
      offset[i] = value.Get<double>();
      transform_set = true;
    }
    transform->set_offset(offset);
  }
  const auto &tex_coord = o.find("texCoord");
  if (tex_coord != o.end()) {
    const tinygltf::Value &value = tex_coord->second;
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
                                    Material *output_material) {
  const tinygltf::Material &input_material =
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
  const tinygltf::PbrMetallicRoughness &pbr =
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

  return OkStatus();
}

void GltfDecoder::DecodeMaterialUnlitExtension(
    const tinygltf::Material &input_material, Material *output_material) {
  // Do nothing if extension is absent.
  const auto &extension_it =
      input_material.extensions.find("KHR_materials_unlit");
  if (extension_it == input_material.extensions.end()) {
    return;
  }

  // Set the unlit property in Draco material.
  output_material->SetUnlit(true);
}

Status GltfDecoder::DecodeMaterialSheenExtension(
    const tinygltf::Material &input_material, Material *output_material) {
  // Do nothing if extension is absent.
  const auto &extension_it =
      input_material.extensions.find("KHR_materials_sheen");
  if (extension_it == input_material.extensions.end()) {
    return OkStatus();
  }

  output_material->SetHasSheen(true);
  const tinygltf::Value::Object &extension_object =
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
    const tinygltf::Material &input_material, Material *output_material) {
  // Do nothing if extension is absent.
  const auto &extension_it =
      input_material.extensions.find("KHR_materials_transmission");
  if (extension_it == input_material.extensions.end()) {
    return OkStatus();
  }

  output_material->SetHasTransmission(true);
  const tinygltf::Value::Object &extension_object =
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
    const tinygltf::Material &input_material, Material *output_material) {
  // Do nothing if extension is absent.
  const auto &extension_it =
      input_material.extensions.find("KHR_materials_clearcoat");
  if (extension_it == input_material.extensions.end()) {
    return OkStatus();
  }

  output_material->SetHasClearcoat(true);
  const tinygltf::Value::Object &extension_object =
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
    const tinygltf::Material &input_material, int input_material_index,
    Material *output_material) {
  // Do nothing if extension is absent.
  const auto &extension_it =
      input_material.extensions.find("KHR_materials_volume");
  if (extension_it == input_material.extensions.end()) {
    return OkStatus();
  }

  output_material->SetHasVolume(true);
  const tinygltf::Value::Object &extension_object =
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
        const std::vector<float> &scales =
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
    const tinygltf::Material &input_material, Material *output_material) {
  // Do nothing if extension is absent.
  const auto &extension_it =
      input_material.extensions.find("KHR_materials_ior");
  if (extension_it == input_material.extensions.end()) {
    return OkStatus();
  }

  output_material->SetHasIor(true);
  const tinygltf::Value::Object &extension_object =
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
    const tinygltf::Material &input_material, Material *output_material) {
  // Do nothing if extension is absent.
  const auto &extension_it =
      input_material.extensions.find("KHR_materials_specular");
  if (extension_it == input_material.extensions.end()) {
    return OkStatus();
  }

  output_material->SetHasSpecular(true);
  const tinygltf::Value::Object &extension_object =
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

  // Decode speclar texture.
  DRACO_RETURN_IF_ERROR(DecodeTexture("specularTexture", TextureMap::SPECULAR,
                                      extension_object, output_material));

  // Decode specular color texture.
  DRACO_RETURN_IF_ERROR(DecodeTexture("specularColorTexture",
                                      TextureMap::SPECULAR_COLOR,
                                      extension_object, output_material));

  return OkStatus();
}

StatusOr<bool> GltfDecoder::DecodeFloat(const std::string &name,
                                        const tinygltf::Value::Object &object,
                                        float *value) {
  const auto &it = object.find(name);
  if (it == object.end()) {
    return false;
  }
  const tinygltf::Value &number = it->second;
  if (!number.IsNumber()) {
    return Status(Status::DRACO_ERROR, "Invalid " + name + ".");
  }
  *value = number.Get<double>();
  return true;
}

StatusOr<bool> GltfDecoder::DecodeVector3f(
    const std::string &name, const tinygltf::Value::Object &object,
    Vector3f *value) {
  const auto &it = object.find(name);
  if (it == object.end()) {
    return false;
  }
  const tinygltf::Value &array = it->second;
  if (!array.IsArray() || array.Size() != 3) {
    return Status(Status::DRACO_ERROR, "Invalid " + name + ".");
  }
  for (int i = 0; i < array.Size(); i++) {
    const tinygltf::Value &array_entry = array.Get(i);
    if (!array_entry.IsNumber()) {
      return Status(Status::DRACO_ERROR, "Invalid " + name + ".");
    }
    (*value)[i] = array_entry.Get<double>();
  }
  return true;
}

Status GltfDecoder::DecodeTexture(const std::string &name,
                                  TextureMap::Type type,
                                  const tinygltf::Value::Object &object,
                                  Material *material) {
  tinygltf::TextureInfo info;
  DRACO_RETURN_IF_ERROR(ParseTextureInfo(name, object, &info));
  DRACO_RETURN_IF_ERROR(CheckAndAddTextureToDracoMaterial(
      info.index, info.texCoord, info.extensions, material, type));
  return OkStatus();
}

Status GltfDecoder::ParseTextureInfo(
    const std::string &texture_name,
    const tinygltf::Value::Object &container_object,
    tinygltf::TextureInfo *texture_info) {
  // Note that tinygltf only parses material textures and not material extension
  // textures. This method mimics the behavior of tinygltf's private function
  // ParseTextureInfo() in order for Draco to decode extension textures.

  // Do nothing if texture with such name is absent.
  const auto &texture_object_it = container_object.find(texture_name);
  if (texture_object_it == container_object.end()) {
    return OkStatus();
  }

  const tinygltf::Value::Object &texture_object =
      texture_object_it->second.Get<tinygltf::Value::Object>();

  // Decode texture index.
  const auto &index_it = texture_object.find("index");
  if (index_it != texture_object.end()) {
    const tinygltf::Value &value = index_it->second;
    if (!value.IsNumber()) {
      return Status(Status::DRACO_ERROR, "Invalid texture index.");
    }
    texture_info->index = value.Get<int>();
  }

  // Decode texture coordinate index.
  const auto &tex_coord_it = texture_object.find("texCoord");
  if (tex_coord_it != texture_object.end()) {
    const tinygltf::Value &value = tex_coord_it->second;
    if (!value.IsInt()) {
      return Status(Status::DRACO_ERROR, "Invalid texture texCoord.");
    }
    texture_info->texCoord = value.Get<int>();
  }

  // Decode texture extensions.
  const auto &extensions_it = texture_object.find("extensions");
  if (extensions_it != texture_object.end()) {
    const tinygltf::Value &extensions = extensions_it->second;
    if (!extensions.IsObject()) {
      return Status(Status::DRACO_ERROR, "Invalid extension.");
    }
    for (const std::string &key : extensions.Keys()) {
      texture_info->extensions[key] = extensions.Get(key);
    }
  }

  // Decode texture extras.
  const auto &extras_it = texture_object.find("extras");
  if (extras_it != texture_object.end()) {
    texture_info->extras = extras_it->second;
  }

  return OkStatus();
}

Status GltfDecoder::AddMaterialsToScene() {
  for (int input_material_index = 0;
       input_material_index < gltf_model_.materials.size();
       ++input_material_index) {
    Material *const output_material =
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
    MeshGroup *const mg = scene_->GetMeshGroup(mgi);
    for (int mi = 0; mi < mg->NumMaterialIndices(); ++mi) {
      const int material_index = mg->GetMaterialIndex(mi);
      if (material_index == -1) {
        mg->SetMaterialIndex(mi, default_material_index);
        default_material_needed = true;
      }
    }
  }
  if (default_material_needed) {
    // Create an empty default material (our defaults correspond to glTF
    // defaults).
    scene_->GetMaterialLibrary().MutableMaterial(default_material_index);
  }

  std::unordered_set<Mesh *> meshes_that_need_tangents;
  // Check if we need to generate tangent space for any of the loaded meshes.
  for (MeshGroupIndex mgi(0); mgi < scene_->NumMeshGroups(); ++mgi) {
    const MeshGroup *const mg = scene_->GetMeshGroup(mgi);
    for (int mi = 0; mi < mg->NumMaterialIndices(); ++mi) {
      const int material_index = mg->GetMaterialIndex(mi);
      const auto tangent_map =
          scene_->GetMaterialLibrary()
              .GetMaterial(material_index)
              ->GetTextureMapByType(TextureMap::NORMAL_TANGENT_SPACE);
      if (tangent_map != nullptr) {
        const MeshIndex mesh_index = mg->GetMeshIndex(mi);
        Mesh &mesh = scene_->GetMesh(mesh_index);
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
    const tinygltf::Skin &skin = gltf_model_.skins[source_skin_index];
    const SkinIndex skin_index = scene_->AddSkin();
    Skin *const new_skin = scene_->GetSkin(skin_index);

    // The skin index was set previously while processing the nodes.
    if (skin_index.value() != source_skin_index) {
      return Status(Status::DRACO_ERROR, "Skin indices are mismatched.");
    }

    if (skin.inverseBindMatrices >= 0) {
      const tinygltf::Accessor &accessor =
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

bool GltfDecoder::PrimitiveSignature::operator==(
    const PrimitiveSignature &signature) const {
  return primitive.indices == signature.primitive.indices &&
         primitive.attributes == signature.primitive.attributes &&
         primitive.extras == signature.primitive.extras &&
         primitive.extensions == signature.primitive.extensions &&
         primitive.mode == signature.primitive.mode &&
         primitive.targets == signature.primitive.targets;
}

size_t GltfDecoder::PrimitiveSignature::Hash::operator()(
    const PrimitiveSignature &signature) const {
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

}  // namespace draco

#endif  // DRACO_TRANSCODER_SUPPORTED
