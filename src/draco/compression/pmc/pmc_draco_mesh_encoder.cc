// Copyright 2026 The Draco Authors.
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
#include "draco/compression/pmc/pmc_draco_mesh_encoder.h"

#ifdef DRACO_PMC_SUPPORTED

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <pmc/pmCodec.hpp>
#include <pmc/pmEncoder.hpp>
#include <pmc/pmTypesAndConstants.hpp>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <vector>

#include "draco/attributes/geometry_attribute.h"
#include "draco/attributes/geometry_indices.h"
#include "draco/attributes/point_attribute.h"
#include "draco/compression/draco_compression_options.h"
#include "draco/compression/pmc/pmc_utils.h"
#include "draco/core/bounding_box.h"
#include "draco/core/draco_types.h"
#include "draco/core/encoder_buffer.h"
#include "draco/core/status.h"
#include "draco/core/vector_d.h"
#include "draco/mesh/mesh.h"

namespace draco {

namespace {

void PopulateAttributeIndices(const Mesh& mesh, const PointAttribute& att,
                              std::vector<int32_t>& indices) {
  const size_t num_faces = mesh.num_faces();
  indices.resize(3 * num_faces);
  for (draco::FaceIndex fi(0); fi < num_faces; ++fi) {
    const draco::Mesh::Face& face = mesh.face(fi);
    for (int j = 0; j < 3; j++) {
      const draco::PointIndex pi = face[j];
      const draco::AttributeValueIndex avi = att.mapped_index(pi);
      indices[3 * fi.value() + j] = avi.value();
    }
  }
}

template <typename ScalarT, int dimension_t>
float PopulateAttribute(const PointAttribute& att, const Mesh& mesh,
                        int quantization_bits,
                        BoundingBoxD<float, dimension_t>& bbox,
                        std::vector<int32_t>& values,
                        std::vector<int32_t>& indices) {
  float scale = 0.0f;
  values.resize(dimension_t * att.size());
  if constexpr (std::is_floating_point_v<ScalarT>) {
    bbox = PmcUtils::ComputeBoundingBox<float, dimension_t>(att);
    scale = PmcUtils::ComputeQuantizationScale(bbox, quantization_bits);
  }
  for (draco::AttributeValueIndex avi(0); avi < att.size(); avi++) {
    std::array<ScalarT, dimension_t> value;
    att.GetValue(avi, value.data());
    for (int i = 0; i < dimension_t; ++i) {
      if constexpr (std::is_floating_point_v<ScalarT>) {
        // Quantize attribute value.
        values[dimension_t * avi.value() + i] =
            PmcUtils::Quantize(value[i], bbox.GetMinPoint()[i], scale);
      } else {
        // Copy attribute value.
        values[dimension_t * avi.value() + i] = value[i];
      }
    }
  }
  PopulateAttributeIndices(mesh, att, indices);
  return scale;
}

void PopulateOctahedralNormals(const PointAttribute& att, const Mesh& mesh,
                               int quantization_bits,
                               std::vector<int32_t>& values,
                               std::vector<int32_t>& indices) {
  values.resize(3 * att.size());
  for (draco::AttributeValueIndex avi(0); avi < att.size(); avi++) {
    std::array<float, 3> value;
    att.GetValue(avi, value.data());
    PmcUtils::QuantizeOctahedralNormal(value.data(), quantization_bits,
                                       &values[3 * avi.value()]);
  }
  PopulateAttributeIndices(mesh, att, indices);
}

Status CheckAttributes(const Mesh& mesh) {
  // Get supported mesh attributes.
  const draco::PointAttribute* normal_att =
      mesh.GetNamedAttribute(draco::GeometryAttribute::NORMAL);
  const draco::PointAttribute* tangent_att =
      mesh.GetNamedAttribute(draco::GeometryAttribute::TANGENT);
  const draco::PointAttribute* color_att =
      mesh.GetNamedAttribute(draco::GeometryAttribute::COLOR);
  const draco::PointAttribute* weight_att =
      mesh.GetNamedAttribute(draco::GeometryAttribute::WEIGHTS);
  const draco::PointAttribute* joint_att =
      mesh.GetNamedAttribute(draco::GeometryAttribute::JOINTS);

  // Check attribute data types and dimensions.
  // TODO(vytyaz): Support more glTF attribute data types and dimensions.

  // Check texture coordinate attributes.
  const int num_texcoord_atts =
      mesh.NumNamedAttributes(draco::GeometryAttribute::TEX_COORD);
  for (int i = 0; i < num_texcoord_atts; ++i) {
    const draco::PointAttribute* texcoord_att =
        mesh.GetNamedAttribute(draco::GeometryAttribute::TEX_COORD, i);
    if (texcoord_att != nullptr) {
      if (texcoord_att->num_components() != 2) {
        return ErrorStatus(
            "Texture coordinate attribute must have 2 components.");
      }
      if (texcoord_att->data_type() != DataType::DT_FLOAT32) {
        return ErrorStatus(
            "Texture coordinate attribute must have float32 type.");
      }
    }
  }

  // Check normal attribute.
  if (normal_att != nullptr) {
    if (normal_att->num_components() != 3) {
      return ErrorStatus("Normal attribute must have 3 components.");
    }
    if (normal_att->data_type() != DataType::DT_FLOAT32) {
      return ErrorStatus("Normal attribute must have float32 type.");
    }
  }

  // Check tangent attribute.
  if (tangent_att != nullptr) {
    if (tangent_att->num_components() != 4) {
      return ErrorStatus(
          "Tangent attribute must have 4 components for PMC encoding.");
    }
    if (tangent_att->data_type() != DataType::DT_FLOAT32) {
      return ErrorStatus("Tangent attribute must have float32 type.");
    }
  }

  // Check color attribute.
  if (color_att != nullptr) {
    if (color_att->num_components() != 3 && color_att->num_components() != 4) {
      return ErrorStatus("Color attribute must have 3 or 4 components.");
    }
    if (color_att->data_type() != DataType::DT_UINT8 &&
        color_att->data_type() != DataType::DT_FLOAT32) {
      return ErrorStatus("Color attribute must have uint8 or float32 type.");
    }
  }

  // Check joints attribute.
  if (joint_att != nullptr) {
    if (joint_att->num_components() != 4) {
      return ErrorStatus("Joint attribute must have 4 components.");
    }
    if (joint_att->data_type() != DataType::DT_UINT16) {
      return ErrorStatus("Joint attribute must have uint16 type.");
    }
  }

  // Check weight attribute.
  if (weight_att != nullptr) {
    if (weight_att->num_components() != 4) {
      return ErrorStatus("Weight attribute must have 4 components.");
    }
    if (weight_att->data_type() != DataType::DT_FLOAT32) {
      return ErrorStatus("Weight attribute must have float32 type.");
    }
  }

  return OkStatus();
}

struct TexCoordData {
  const draco::PointAttribute* att = nullptr;
  std::vector<int32_t> values;
  std::vector<int32_t> indices;
  BoundingBoxD<float, 2> bbox;
  float scale = 0.0f;
  pmc::AttributeMeshpart meshpart;
};

}  // namespace

Status PmcDracoMeshEncoder::Encode(const Mesh& mesh,
                                   EncoderBuffer* out_buffer) {
  // Get quantization bits from mesh compression options.
  const DracoCompressionOptions& options = mesh.GetCompressionOptions();
  if (!options.quantization_position.AreQuantizationBitsDefined()) {
    // TODO(vytyaz): Support global quantization grid.
    return ErrorStatus("Global quantization grid not yet supported.");
  }
  const int position_quantization_bits =
      options.quantization_position.quantization_bits();

  // Get and check position attribute.
  const draco::PointAttribute* position_att =
      mesh.GetNamedAttribute(draco::GeometryAttribute::POSITION);
  if (position_att == nullptr) {
    return ErrorStatus("Mesh must have position attribute.");
  }

  // TODO(vytyaz): Encode position attribute unique ID in user data.
  if (position_att->unique_id() != 0) {
    // Right now non-zero unique attribute ID for position attribute may be
    // changed after encoding and decoding with PMC PMC.
  }

  // Allocate vectors for positions, face degrees, and vertex indices.
  // TODO(vytyaz): Revisit allocation of face degrees in case PMC allows
  // implicit face degrees for triangular meshes in the future.
  const size_t num_faces = mesh.num_faces();
  const size_t num_corners = 3 * num_faces;
  const size_t num_positions = position_att->size();
  std::vector<int32_t> positions(3 * num_positions);  // (x, y, z).
  std::vector<int32_t> face_degrees(num_faces, 3);
  std::vector<int32_t> indices(num_corners);

  // Populate positions vector with quantized position values.
  const BoundingBox position_bbox = mesh.ComputeBoundingBox();
  const float scale = PmcUtils::ComputeQuantizationScale(
      position_bbox, position_quantization_bits);
  for (draco::AttributeValueIndex avi(0); avi < num_positions; avi++) {
    std::array<float, 3> pos;
    position_att->GetValue(avi, pos.data());
    for (int i = 0; i < 3; ++i) {
      positions[3 * avi.value() + i] =
          PmcUtils::Quantize(pos[i], position_bbox.GetMinPoint()[i], scale);
    }
  }

  // Populate indices.
  for (draco::FaceIndex fi(0); fi < num_faces; ++fi) {
    const draco::Mesh::Face& face = mesh.face(fi);
    for (int j = 0; j < 3; j++) {
      const draco::PointIndex pi = face[j];
      const draco::AttributeValueIndex avi = position_att->mapped_index(pi);
      indices[3 * fi.value() + j] = avi.value();
    }
  }

  // Create and configure PMC PMC encoder.
  pmc::Encoder encoder;
  pmc::Error ret;

  // Configure PMC PMC coordinate system for geometry positions.
  pmc::GeometryInfo geometry_info = PmcUtils::CreateGeometryInfo(
      position_quantization_bits, position_bbox.GetMinPoint(), scale);
  ret = encoder.configure(geometry_info);
  if (ret != pmc::Error::OK) {
    return ErrorStatus("Failed to configure geometry info.");
  }

  // Get supported mesh attributes.
  // TODO(vytyaz): Support multiple attributes of the same type as allowed by
  // glTF, such as color, joints, and weights.
  const int num_texcoord_atts =
      mesh.NumNamedAttributes(draco::GeometryAttribute::TEX_COORD);
  std::vector<std::unique_ptr<TexCoordData>> texcoord_datas;
  for (int i = 0; i < num_texcoord_atts; ++i) {
    const draco::PointAttribute* att =
        mesh.GetNamedAttribute(draco::GeometryAttribute::TEX_COORD, i);
    if (att != nullptr) {
      auto data = std::make_unique<TexCoordData>();
      data->att = att;
      texcoord_datas.push_back(std::move(data));
    }
  }
  const draco::PointAttribute* normal_att =
      mesh.GetNamedAttribute(draco::GeometryAttribute::NORMAL);
  const draco::PointAttribute* tangent_att =
      mesh.GetNamedAttribute(draco::GeometryAttribute::TANGENT);
  const draco::PointAttribute* color_att =
      mesh.GetNamedAttribute(draco::GeometryAttribute::COLOR);
  const draco::PointAttribute* weight_att =
      mesh.GetNamedAttribute(draco::GeometryAttribute::WEIGHTS);
  const draco::PointAttribute* joint_att =
      mesh.GetNamedAttribute(draco::GeometryAttribute::JOINTS);

  // Get quantization bits for attributes.
  int tex_coord_quantization_bits = options.quantization_bits_tex_coord;
  int normal_quantization_bits = options.quantization_bits_normal;
  int tangent_quantization_bits = options.quantization_bits_tangent;
  int weight_quantization_bits = options.quantization_bits_weight;
  int color_quantization_bits = options.quantization_bits_color;

  // Collect information about which attributes are present.
  const bool has_tangents = tangent_att != nullptr;
  const bool has_colors = color_att != nullptr;
  const bool has_weights = weight_att != nullptr;
  const bool has_joints = joint_att != nullptr;
  const bool has_normals = normal_att != nullptr;

  // Check mesh attribute types and dimensions.
  DRACO_RETURN_IF_ERROR(CheckAttributes(mesh));

  // Create normal encoding parameters early to check for octahedral normals.
  // Increment normal quantization bits if using octahedral normals.
  pmc::AttributeEncodingParameters normal_params;
  bool use_octahedral_normals = false;
  if (has_normals) {
    normal_params = PmcUtils::CreateNormalAttributeEncodingParameters();
    use_octahedral_normals =
        normal_params.predictionStrategy ==
        pmc::PredictionStrategy::UNITARY_OCTAHEDRAL_NORMAL_VECTOR;
    if (use_octahedral_normals) {
      normal_quantization_bits += 1;
    }
  }

  // Vectors for attribute values.
  std::vector<int32_t> normal_values;
  std::vector<int32_t> tangent_values;
  std::vector<int32_t> color_values;
  std::vector<int32_t> weight_values;
  std::vector<int32_t> joint_values;

  // Vectors for attribute indices.
  std::vector<int32_t> normal_indices;
  std::vector<int32_t> tangent_indices;
  std::vector<int32_t> color_indices;
  std::vector<int32_t> weight_indices;
  std::vector<int32_t> joint_indices;

  // Populate texture coordinate values and indices.
  for (auto& data : texcoord_datas) {
    data->scale = PopulateAttribute<float, 2>(
        *data->att, mesh, tex_coord_quantization_bits, data->bbox, data->values,
        data->indices);
  }

  // Populate normal values and indices.
  BoundingBoxD<float, 3> normal_bbox;
  float normal_scale = 0.0f;
  if (has_normals) {
    if (use_octahedral_normals) {
      PopulateOctahedralNormals(*normal_att, mesh, normal_quantization_bits,
                                normal_values, normal_indices);
    } else {
      normal_scale = PopulateAttribute<float, 3>(
          *normal_att, mesh, normal_quantization_bits, normal_bbox,
          normal_values, normal_indices);
    }
  }

  // Populate tangent values and indices.
  BoundingBoxD<float, 4> tangent_bbox;
  float tangent_scale = 0.0f;
  if (has_tangents) {
    tangent_scale = PopulateAttribute<float, 4>(
        *tangent_att, mesh, tangent_quantization_bits, tangent_bbox,
        tangent_values, tangent_indices);
  }

  // Populate color values and indices.
  float color_scale = 0.0f;
  std::array<float, 4> color_min_vals = {0, 0, 0, 0};
  if (has_colors) {
    if (color_att->data_type() == DataType::DT_FLOAT32) {
      if (color_att->num_components() == 3) {
        auto color_bbox = PmcUtils::ComputeBoundingBox<float, 3>(*color_att);
        color_scale = PopulateAttribute<float, 3>(
            *color_att, mesh, color_quantization_bits, color_bbox, color_values,
            color_indices);
        for (int i = 0; i < 3; ++i) {
          color_min_vals[i] = color_bbox.GetMinPoint()[i];
        }
      } else {
        auto color_bbox = PmcUtils::ComputeBoundingBox<float, 4>(*color_att);
        color_scale = PopulateAttribute<float, 4>(
            *color_att, mesh, color_quantization_bits, color_bbox, color_values,
            color_indices);
        for (int i = 0; i < 4; ++i) {
          color_min_vals[i] = color_bbox.GetMinPoint()[i];
        }
      }
    } else {
      // For DataType::DT_UINT8 color quantization bits are always 8.
      color_quantization_bits = 8;
      if (color_att->num_components() == 3) {
        BoundingBoxD<float, 3> color_bbox;
        PopulateAttribute<uint8_t, 3>(*color_att, mesh, color_quantization_bits,
                                      color_bbox, color_values, color_indices);
      } else {
        BoundingBoxD<float, 4> color_bbox;
        PopulateAttribute<uint8_t, 4>(*color_att, mesh, color_quantization_bits,
                                      color_bbox, color_values, color_indices);
      }
    }
  }

  // Populate weight values and indices.
  BoundingBoxD<float, 4> weight_bbox;
  float weight_scale = 0.0f;
  if (has_weights) {
    weight_scale =
        PopulateAttribute<float, 4>(*weight_att, mesh, weight_quantization_bits,
                                    weight_bbox, weight_values, weight_indices);
  }

  // Populate joint values and indices.
  BoundingBoxD<float, 4> joint_bbox;
  if (has_joints) {
    constexpr int kJointQuantizationBits = 0;
    PopulateAttribute<uint16_t, 4>(*joint_att, mesh, kJointQuantizationBits,
                                   joint_bbox, joint_values, joint_indices);
  }

  // Create geometry mesh part.
  pmc::GeometryMeshpart geometry_meshpart = PmcUtils::CreateGeometryMeshpart(
      positions, face_degrees, indices, position_quantization_bits);

  // Create texture coordinate mesh parts.
  for (auto& data : texcoord_datas) {
    data->meshpart = PmcUtils::CreateAttributeMeshpart(
        data->values, data->indices, data->att->unique_id(), 2,
        pmc::AttributeType::TEX_COORD, pmc::AttributeScope::CORNER,
        tex_coord_quantization_bits);
  }

  // Create normal mesh part.
  pmc::AttributeMeshpart normal_meshpart;
  if (has_normals) {
    normal_meshpart = PmcUtils::CreateAttributeMeshpart(
        normal_values, normal_indices, normal_att->unique_id(), 3,
        pmc::AttributeType::NORMAL, pmc::AttributeScope::CORNER,
        normal_quantization_bits, use_octahedral_normals);
  }

  // Create tangent mesh part.
  pmc::AttributeMeshpart tangent_meshpart;
  if (has_tangents) {
    tangent_meshpart = PmcUtils::CreateAttributeMeshpart(
        tangent_values, tangent_indices, tangent_att->unique_id(), 4,
        pmc::AttributeType::TANGENT, pmc::AttributeScope::CORNER,
        tangent_quantization_bits);
  }

  // Create color mesh part.
  pmc::AttributeMeshpart color_meshpart;
  if (has_colors) {
    color_meshpart = PmcUtils::CreateAttributeMeshpart(
        color_values, color_indices, color_att->unique_id(),
        color_att->num_components(), pmc::AttributeType::COLOR,
        pmc::AttributeScope::CORNER, color_quantization_bits);
  }

  // Create weight mesh part.
  pmc::AttributeMeshpart weight_meshpart;
  if (has_weights) {
    weight_meshpart = PmcUtils::CreateAttributeMeshpart(
        weight_values, weight_indices, weight_att->unique_id(), 4,
        pmc::AttributeType::WEIGHT, pmc::AttributeScope::CORNER,
        weight_quantization_bits);
  }

  // Create joint mesh part.
  pmc::AttributeMeshpart joint_meshpart;
  if (has_joints) {
    constexpr int kJointQuantizationBits = 0;
    joint_meshpart = PmcUtils::CreateAttributeMeshpart(
        joint_values, joint_indices, joint_att->unique_id(), 4,
        pmc::AttributeType::JOINT, pmc::AttributeScope::CORNER,
        kJointQuantizationBits);
  }

  // Create position encoding parameters.
  pmc::GeometryEncodingParameters geom_params =
      PmcUtils::CreateGeometryEncodingParameters();

  // Create attribute encoding parameters.
  pmc::AttributeEncodingParameters texcoord_params;
  pmc::AttributeEncodingParameters tangent_params;
  pmc::AttributeEncodingParameters color_params;
  pmc::AttributeEncodingParameters weight_params;
  pmc::AttributeEncodingParameters joint_params;
  if (!texcoord_datas.empty()) {
    texcoord_params = PmcUtils::CreateTexCoordAttributeEncodingParameters();
  }
  if (has_normals) {
    // Normal parameters are already created.
  }
  if (has_tangents) {
    tangent_params = PmcUtils::CreateGenericAttributeEncodingParameters();
  }
  if (has_colors) {
    color_params = PmcUtils::CreateGenericAttributeEncodingParameters();
  }
  if (has_weights) {
    weight_params = PmcUtils::CreateGenericAttributeEncodingParameters();
  }
  if (has_joints) {
    joint_params = PmcUtils::CreateGenericAttributeEncodingParameters();
  }

  // Estimate the upper bound of the bitstream size.
  auto estimateSize = [](const auto& meshpart, const auto& params) {
    return pmc::Encoder::estimateMaxEncodedSize(meshpart, params);
  };
  size_t buffer_size = estimateSize(geometry_meshpart, geom_params);
  for (const auto& data : texcoord_datas) {
    buffer_size += estimateSize(data->meshpart, texcoord_params);
  }
  if (has_normals) {
    buffer_size += estimateSize(normal_meshpart, normal_params);
  }
  if (has_tangents) {
    buffer_size += estimateSize(tangent_meshpart, tangent_params);
  }
  if (has_colors) {
    buffer_size += estimateSize(color_meshpart, color_params);
  }
  if (has_weights) {
    buffer_size += estimateSize(weight_meshpart, weight_params);
  }
  if (has_joints) {
    buffer_size += estimateSize(joint_meshpart, joint_params);
  }

  // Allocate memory for the bitstream.
  std::vector<uint8_t> bitstream_buffer;
  bitstream_buffer.resize(buffer_size);
  pmc::ByteBuffer buffer;
  buffer.data = bitstream_buffer.data();
  buffer.capacity = bitstream_buffer.size();
  buffer.size = 0;

  // Helper to get an attribute name from metadata or use fallback name.
  auto get_attribute_name = [&](const draco::PointAttribute* att,
                                const std::string& fallback) {
    if (att != nullptr) {
      const int32_t att_id = mesh.GetAttributeIdByUniqueId(att->unique_id());
      if (att_id != -1) {
        const auto* metadata = mesh.GetAttributeMetadataByAttributeId(att_id);
        std::string attribute_name;
        if (metadata != nullptr &&
            metadata->GetEntryString("attribute_name", &attribute_name) &&
            !attribute_name.empty()) {
          return attribute_name;
        }
      }
    }
    return fallback;
  };

  // Helper to verify that all attribute names are unique. Position attribute
  // is always present and its name is always "POSITION".
  std::unordered_set<std::string> encoded_names = {"POSITION"};
  auto check_and_register_name = [&](const std::string& name) -> Status {
    if (!encoded_names.insert(name).second) {
      return ErrorStatus(
          "Attribute names must be unique. Duplicate name found: " + name);
    }
    return OkStatus();
  };

  // Configure texture coordinate attribute encoding.
  for (size_t i = 0; i < texcoord_datas.size(); ++i) {
    auto& data = texcoord_datas[i];
    const std::string fallback_name = "TEXCOORD_" + std::to_string(i);
    const std::string name = get_attribute_name(data->att, fallback_name);
    DRACO_RETURN_IF_ERROR(check_and_register_name(name));
    ret = encoder.configure(PmcUtils::CreateAttributeInfo(
        data->meshpart.info, data->bbox.GetMinPoint(), data->scale, name));
    if (ret != pmc::Error::OK) {
      return ErrorStatus("Failed to configure texture coordinate attribute.");
    }
  }

  // Configure normal attribute encoding.
  if (has_normals) {
    const std::string name = get_attribute_name(normal_att, "NORMAL");
    DRACO_RETURN_IF_ERROR(check_and_register_name(name));
    if (use_octahedral_normals) {
      const float half_max =
          static_cast<float>((1 << normal_quantization_bits >> 1) - 1);
      const Vector3f min_point(-1.0f, -1.0f, -1.0f);
      ret = encoder.configure(PmcUtils::CreateAttributeInfo(
          normal_meshpart.info, min_point, half_max, name));
    } else {
      ret = encoder.configure(PmcUtils::CreateAttributeInfo(
          normal_meshpart.info, normal_bbox.GetMinPoint(), normal_scale, name));
    }
    if (ret != pmc::Error::OK) {
      return ErrorStatus("Failed to configure normal attribute.");
    }
  }

  // Configure tangent attribute encoding.
  if (has_tangents) {
    const std::string name = get_attribute_name(tangent_att, "TANGENT");
    DRACO_RETURN_IF_ERROR(check_and_register_name(name));
    ret = encoder.configure(PmcUtils::CreateAttributeInfo(
        tangent_meshpart.info, tangent_bbox.GetMinPoint(), tangent_scale,
        name));
    if (ret != pmc::Error::OK) {
      return ErrorStatus("Failed to configure tangent attribute.");
    }
  }

  // Configure color attribute encoding.
  if (has_colors) {
    ret = pmc::Error::OK;
    const std::string name = get_attribute_name(color_att, "COLOR_0");
    DRACO_RETURN_IF_ERROR(check_and_register_name(name));
    if (color_att->num_components() == 3) {
      const Vector3f min_point(color_min_vals[0], color_min_vals[1],
                               color_min_vals[2]);
      ret = encoder.configure(PmcUtils::CreateAttributeInfo(
          color_meshpart.info, min_point, color_scale, name));
    } else {
      const Vector4f min_point(color_min_vals[0], color_min_vals[1],
                               color_min_vals[2], color_min_vals[3]);
      ret = encoder.configure(PmcUtils::CreateAttributeInfo(
          color_meshpart.info, min_point, color_scale, name));
    }
    if (ret != pmc::Error::OK) {
      return ErrorStatus("Failed to configure color attribute.");
    }
  }

  // Configure weight attribute encoding.
  if (has_weights) {
    const std::string name = get_attribute_name(weight_att, "WEIGHTS_0");
    DRACO_RETURN_IF_ERROR(check_and_register_name(name));
    ret = encoder.configure(PmcUtils::CreateAttributeInfo(
        weight_meshpart.info, weight_bbox.GetMinPoint(), weight_scale, name));
    if (ret != pmc::Error::OK) {
      return ErrorStatus("Failed to configure weight attribute.");
    }
  }

  // Configure joint attribute encoding.
  if (has_joints) {
    const Vector4f unused_min_point;
    const std::string name = get_attribute_name(joint_att, "JOINTS_0");
    DRACO_RETURN_IF_ERROR(check_and_register_name(name));
    // No quantization is needed for joints. They are always integers.
    ret = encoder.configure(PmcUtils::CreateAttributeInfo(
        joint_meshpart.info, unused_min_point, 0.0f, name));
    if (ret != pmc::Error::OK) {
      return ErrorStatus("Failed to configure joint attribute.");
    }
  }

  // Encode position.
  ret = encoder.encode(geometry_meshpart, buffer, geom_params);
  if (ret != pmc::Error::OK) {
    return ErrorStatus("Failed to encode geometry meshpart.");
  }

  // Encode other attributes.
  for (const auto& data : texcoord_datas) {
    ret = encoder.encode(data->meshpart, buffer, texcoord_params);
    if (ret != pmc::Error::OK) {
      return ErrorStatus("Failed to encode texture coordinate meshpart.");
    }
  }
  if (has_normals) {
    ret = encoder.encode(normal_meshpart, buffer, normal_params);
    if (ret != pmc::Error::OK) {
      return ErrorStatus("Failed to encode normal meshpart.");
    }
  }
  if (has_tangents) {
    ret = encoder.encode(tangent_meshpart, buffer, tangent_params);
    if (ret != pmc::Error::OK) {
      return ErrorStatus("Failed to encode tangent meshpart.");
    }
  }
  if (has_colors) {
    ret = encoder.encode(color_meshpart, buffer, color_params);
    if (ret != pmc::Error::OK) {
      return ErrorStatus("Failed to encode color meshpart.");
    }
  }
  if (has_weights) {
    ret = encoder.encode(weight_meshpart, buffer, weight_params);
    if (ret != pmc::Error::OK) {
      return ErrorStatus("Failed to encode weight meshpart.");
    }
  }
  if (has_joints) {
    ret = encoder.encode(joint_meshpart, buffer, joint_params);
    if (ret != pmc::Error::OK) {
      return ErrorStatus("Failed to encode joint meshpart.");
    }
  }

  // Copy encoded data to output buffer.
  out_buffer->Encode(buffer.data, buffer.size);
  return OkStatus();
}

}  // namespace draco

#endif  // DRACO_PMC_SUPPORTED
