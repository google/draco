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
#include "draco/compression/pmc/pmc_draco_mesh_decoder.h"

#ifdef DRACO_PMC_SUPPORTED

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <pmc/pmCodec.hpp>
#include <pmc/pmDecoder.hpp>
#include <pmc/pmTypesAndConstants.hpp>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "draco/attributes/geometry_attribute.h"
#include "draco/attributes/geometry_indices.h"
#include "draco/compression/pmc/pmc_utils.h"
#include "draco/core/decoder_buffer.h"
#include "draco/core/draco_types.h"
#include "draco/core/status.h"
#include "draco/core/status_or.h"
#include "draco/core/vector_d.h"
#include "draco/mesh/mesh.h"
#include "draco/mesh/triangle_soup_mesh_builder.h"

namespace draco {

namespace {

// Holds attribute quantization parameters.
template <int dimension_t>
struct QuantizationParams {
  VectorD<float, dimension_t> min_vals;
  float scale;
};

// Checks attribute validity, adds it to the mesh builder, and retrieves
// quantization parameters.
template <int dimension_t>
StatusOr<int> AddAttributeAndRetrieveQuantizationParams(
    GeometryAttribute::Type att_type, DataType data_type,
    int decoded_components_per_vector,
    const pmc::AttributeMeshpartInfo& attr_info,
    const std::vector<int32_t>& indices,
    QuantizationParams<dimension_t>* quantization,
    draco::TriangleSoupMeshBuilder* mb) {
  // Return if this attribute was not decoded.
  if (indices.empty()) {
    return -1;
  }

  // Check that decoded components per vector is valid.
  if (decoded_components_per_vector != dimension_t) {
    return ErrorStatus(GeometryAttribute::TypeToString(att_type) +
                       " attribute must have " + std::to_string(dimension_t) +
                       " components.");
  }

  // Add attribute to mesh builder.
  const int att_id = mb->AddAttribute(att_type, dimension_t, data_type);
  if (att_id < 0) {
    return ErrorStatus("Failed to add " +
                       GeometryAttribute::TypeToString(att_type) +
                       " attribute.");
  }

  // Retrieve quantization parameters from attribute's coordinate system.
  if (attr_info.coordSys.has_value() && attr_info.coordSys->scale.p != 0) {
    quantization->scale = static_cast<float>(attr_info.coordSys->scale);
    for (int k = 0; k < dimension_t; ++k) {
      quantization->min_vals[k] =
          static_cast<float>(attr_info.coordSys->origin[k]) /
          quantization->scale;
    }
  } else {
    quantization->scale = 0.0f;
    for (int k = 0; k < dimension_t; ++k) {
      quantization->min_vals[k] = 0.0f;
    }
  }
  return att_id;
}

// Populates attribute and returns attribute ID.
template <typename ScalarT, int dimension_t>
StatusOr<int> PopulateAttribute(GeometryAttribute::Type att_type,
                                DataType data_type,
                                int decoded_components_per_vector,
                                const pmc::AttributeMeshpartInfo& attr_info,
                                const std::vector<int32_t>& indices,
                                const std::vector<int32_t>& values,
                                int face_count, bool use_octahedral_normals,
                                draco::TriangleSoupMeshBuilder* mb) {
  // Check attribute validity, add it to the mesh builder, and get attribute
  // quantization parameters.
  QuantizationParams<dimension_t> quantization;
  DRACO_ASSIGN_OR_RETURN(const int att_id,
                         AddAttributeAndRetrieveQuantizationParams<dimension_t>(
                             att_type, data_type, decoded_components_per_vector,
                             attr_info, indices, &quantization, mb));
  if (att_id < 0) {
    return -1;
  }

  // Populate attribute values for each face.
  draco::FaceIndex fi(0);
  for (int i = 0; i < face_count; i++) {
    std::array<std::array<ScalarT, dimension_t>, 3> att_values;
    // Loop over triangular face vertices.
    for (int j = 0; j < 3; ++j) {
      const int32_t index = indices[3 * i + j];
      if constexpr (std::is_floating_point_v<ScalarT> && dimension_t == 3) {
        if (use_octahedral_normals) {
          PmcUtils::DequantizeOctahedralNormal(&values[3 * index],
                                               att_values[j].data());
          continue;
        }
      }
      // Loop over attribute components.
      for (int k = 0; k < dimension_t; ++k) {
        if constexpr (std::is_floating_point_v<ScalarT>) {
          // Dequantize attribute value.
          att_values[j][k] = PmcUtils::Dequantize(
              values[dimension_t * index + k], quantization.min_vals[k],
              quantization.scale);
        } else {
          // Copy attribute value.
          att_values[j][k] =
              static_cast<ScalarT>(values[dimension_t * index + k]);
        }
      }
    }
    // Set attribute values for face.
    mb->SetAttributeValuesForFace(att_id, fi++, att_values[0].data(),
                                  att_values[1].data(), att_values[2].data());
  }
  return att_id;
}

template <typename ScalarT, int components_per_vector>
Status PopulateColorAttribute(const pmc::AttributeMeshpartInfo& color_info,
                              const std::vector<int32_t>& color_indices,
                              const std::vector<int32_t>& color_values,
                              int face_count,
                              draco::TriangleSoupMeshBuilder* mb) {
  const DataType data_type =
      std::is_same_v<ScalarT, float> ? draco::DT_FLOAT32 : draco::DT_UINT8;
  return PopulateAttribute<ScalarT, components_per_vector>(
             draco::GeometryAttribute::COLOR, data_type, components_per_vector,
             color_info, color_indices, color_values, face_count, false, mb)
      .status();
}

struct DecodedTexCoord {
  pmc::AttributeMeshpartInfo info;
  std::vector<int32_t> values;
  std::vector<int32_t> indices;
  int att_id = -1;
};

}  // namespace

Status PmcDracoMeshDecoder::Decode(
    DecoderBuffer* in_buffer, Mesh* mesh,
    const std::vector<std::string>& attribute_names) {
  // Wrap PMC PMC buffer around Draco decoder buffer.
  pmc::ByteBuffer buffer;
  buffer.data = const_cast<uint8_t*>(
      reinterpret_cast<const uint8_t*>(in_buffer->data_head()));
  buffer.capacity = in_buffer->remaining_size();
  buffer.size = in_buffer->remaining_size();

  // Allocate decoded geometry and attribute info.
  pmc::GeometryMeshpartInfo geom_info;
  std::vector<std::unique_ptr<DecodedTexCoord>> decoded_texcoords;
  pmc::AttributeMeshpartInfo normal_info;
  pmc::AttributeMeshpartInfo tangent_info;
  pmc::AttributeMeshpartInfo color_info;
  pmc::AttributeMeshpartInfo weight_info;
  pmc::AttributeMeshpartInfo joint_info;

  // Create PMC PMC decoder and inspection delegate.
  pmc::Decoder decoder;
  pmc::Decoder::InspectionDelegate inspect_delegate;

  // Define inspection callback for geometry.
  inspect_delegate.onInspectGeometryMeshpart =
      [&](const pmc::GeometryMeshpartInfo& info, bool& decode) -> pmc::Error {
    geom_info = info;
    decode = true;
    return pmc::Error::OK;
  };

  // Define inspection callback for attributes.
  std::vector<std::string> present_attribute_names;

  inspect_delegate.onInspectAttributeMeshpart =
      [&](const pmc::AttributeMeshpartInfo& info, bool& decode) -> pmc::Error {
    // Only decode attributes that have a name in attribute_names when provided.
    bool is_requested = false;
    if (attribute_names.empty()) {
      is_requested = true;
    } else {
      for (const auto& name : attribute_names) {
        if (name == info.name) {
          is_requested = true;
          break;
        }
      }
    }
    if (!is_requested) {
      decode = false;
      return pmc::Error::OK;
    }

    if (info.type == pmc::AttributeType::TEX_COORD) {
      auto tc = std::make_unique<DecodedTexCoord>();
      tc->info = info;
      decoded_texcoords.push_back(std::move(tc));
    } else if (info.type == pmc::AttributeType::NORMAL) {
      normal_info = info;
    } else if (info.type == pmc::AttributeType::TANGENT) {
      tangent_info = info;
    } else if (info.type == pmc::AttributeType::COLOR) {
      color_info = info;
    } else if (info.type == pmc::AttributeType::WEIGHT) {
      weight_info = info;
    } else if (info.type == pmc::AttributeType::JOINT) {
      joint_info = info;
    } else {
      // Skip unsupported attribute types and continue decoding.
      decode = false;
      return pmc::Error::OK;
    }
    present_attribute_names.push_back(info.name);
    decode = true;
    return pmc::Error::OK;
  };

  // Inspect the PMC PMC buffer.
  pmc::Error ret = decoder.inspect(buffer, inspect_delegate);
  if (ret != pmc::Error::OK) {
    return ErrorStatus("PMC PMC decoding inspection failed.");
  }

  // Geometry meshpart has been inspected, so position attribute is present.
  present_attribute_names.push_back("POSITION");

  // Check that all requested attributes are present.
  if (!attribute_names.empty()) {
    for (const auto& name : attribute_names) {
      bool found = false;
      for (const auto& present_name : present_attribute_names) {
        if (name == present_name) {
          found = true;
          break;
        }
      }
      if (!found) {
        return ErrorStatus("Attribute with name " + name + " not found.");
      }
    }
  }

  // Allocate geometry buffers.
  std::vector<int32_t> positions(3 * geom_info.vertexCount, 0);
  std::vector<int32_t> face_degrees(geom_info.faceCount, 0);
  std::vector<int32_t> indices(geom_info.indexCount, 0);

  // Allocate attribute buffers.
  for (auto& tc : decoded_texcoords) {
    tc->values.resize(tc->info.vectorCount * tc->info.componentsPerVector);
    tc->indices.resize(tc->info.indexCount);
  }
  std::vector<int32_t> normal_values(normal_info.vectorCount *
                                     normal_info.componentsPerVector);
  std::vector<int32_t> tangent_values(tangent_info.vectorCount *
                                      tangent_info.componentsPerVector);
  std::vector<int32_t> color_values(color_info.vectorCount *
                                    color_info.componentsPerVector);
  std::vector<int32_t> weight_values(weight_info.vectorCount *
                                     weight_info.componentsPerVector);
  std::vector<int32_t> joint_values(joint_info.vectorCount *
                                    joint_info.componentsPerVector);
  std::vector<int32_t> normal_indices(normal_info.indexCount);
  std::vector<int32_t> tangent_indices(tangent_info.indexCount);
  std::vector<int32_t> color_indices(color_info.indexCount);
  std::vector<int32_t> weight_indices(weight_info.indexCount);
  std::vector<int32_t> joint_indices(joint_info.indexCount);

  // Define geometry decoding callbacks.
  pmc::Decoder::DecodingDelegate decode_delegate;
  decode_delegate.onStartGeometryMeshpartDecoding =
      [&](const pmc::GeometryMeshpartInfo& info,
          pmc::GeometryMeshpartBuffers& buffers) -> pmc::Error {
    if (info.connectivityRegularized) {
      // Reserve space for triangle faces restored from regularized quad faces.
      indices.resize(info.indexCount / 2 * 3);
      face_degrees.resize(info.faceCount * 2);
    }
    buffers.positions = PmcUtils::CreateArrayBuffer(positions, 3);
    buffers.faceDegrees = PmcUtils::CreateArrayBuffer(face_degrees, 1);
    buffers.indices = PmcUtils::CreateArrayBuffer(indices, 1);
    return pmc::Error::OK;
  };
  int32_t byte_count = -1;
  decode_delegate.onEndGeometryMeshpartDecoding =
      [&](const pmc::GeometryMeshpart& geometry_meshpart,
          size_t decodedByteCount) -> pmc::Error {
    // Remove potential dummy faces.
    geom_info = geometry_meshpart.info;
    indices.resize(geom_info.indexCount);
    face_degrees.resize(geom_info.faceCount);
    byte_count = decodedByteCount;
    return pmc::Error::OK;
  };

  // Define attribute decoding callbacks.
  decode_delegate.onStartAttributeMeshpartDecoding =
      [&](const pmc::AttributeMeshpartInfo& info,
          const pmc::GeometryMeshpartInfo& geom_info,
          pmc::AttributeMeshpartBuffers& buffers) -> pmc::Error {
    if (geom_info.connectivityRegularized) {
      if (info.scope == pmc::AttributeScope::CORNER) {
        if (info.type == pmc::AttributeType::TEX_COORD) {
          for (auto& tc : decoded_texcoords) {
            if (tc->info.name == info.name) {
              tc->indices.resize(geom_info.indexCount);
              break;
            }
          }
        } else if (info.type == pmc::AttributeType::NORMAL) {
          normal_indices.resize(geom_info.indexCount);
        } else if (info.type == pmc::AttributeType::TANGENT) {
          tangent_indices.resize(geom_info.indexCount);
        } else if (info.type == pmc::AttributeType::COLOR) {
          color_indices.resize(geom_info.indexCount);
        } else if (info.type == pmc::AttributeType::WEIGHT) {
          weight_indices.resize(geom_info.indexCount);
        } else if (info.type == pmc::AttributeType::JOINT) {
          joint_indices.resize(geom_info.indexCount);
        }
      }
    }

    if (info.type == pmc::AttributeType::TEX_COORD) {
      for (auto& tc : decoded_texcoords) {
        if (tc->info.name == info.name) {
          buffers.values = PmcUtils::CreateArrayBuffer(tc->values, 2);
          buffers.indices = PmcUtils::CreateArrayBuffer(tc->indices, 1);
          break;
        }
      }
    } else if (info.type == pmc::AttributeType::NORMAL) {
      buffers.values = PmcUtils::CreateArrayBuffer(normal_values, 3);
      buffers.indices = PmcUtils::CreateArrayBuffer(normal_indices, 1);
    } else if (info.type == pmc::AttributeType::TANGENT) {
      buffers.values = PmcUtils::CreateArrayBuffer(tangent_values, 4);
      buffers.indices = PmcUtils::CreateArrayBuffer(tangent_indices, 1);
    } else if (info.type == pmc::AttributeType::COLOR) {
      buffers.values = PmcUtils::CreateArrayBuffer(
          color_values, color_info.componentsPerVector);
      buffers.indices = PmcUtils::CreateArrayBuffer(color_indices, 1);
    } else if (info.type == pmc::AttributeType::WEIGHT) {
      buffers.values = PmcUtils::CreateArrayBuffer(weight_values, 4);
      buffers.indices = PmcUtils::CreateArrayBuffer(weight_indices, 1);
    } else if (info.type == pmc::AttributeType::JOINT) {
      buffers.values = PmcUtils::CreateArrayBuffer(joint_values, 4);
      buffers.indices = PmcUtils::CreateArrayBuffer(joint_indices, 1);
    } else {
      // The unsupported attribute types are skipped, as requested in the
      // |inspect_delegate.onInspectAttributeMeshpart()|.
    }
    return pmc::Error::OK;
  };
  decode_delegate.onEndAttributeMeshpartDecoding =
      [&](const pmc::AttributeMeshpart& attributeMeshpart,
          size_t decodedByteCount) -> pmc::Error {
    if (attributeMeshpart.info.type == pmc::AttributeType::TEX_COORD) {
      for (auto& tc : decoded_texcoords) {
        if (tc->info.name == attributeMeshpart.info.name) {
          tc->info = attributeMeshpart.info;
          break;
        }
      }
    } else if (attributeMeshpart.info.type == pmc::AttributeType::NORMAL) {
      normal_info = attributeMeshpart.info;
    } else if (attributeMeshpart.info.type == pmc::AttributeType::TANGENT) {
      tangent_info = attributeMeshpart.info;
    } else if (attributeMeshpart.info.type == pmc::AttributeType::COLOR) {
      color_info = attributeMeshpart.info;
    } else if (attributeMeshpart.info.type == pmc::AttributeType::WEIGHT) {
      weight_info = attributeMeshpart.info;
    } else if (attributeMeshpart.info.type == pmc::AttributeType::JOINT) {
      joint_info = attributeMeshpart.info;
    } else {
      // The unsupported attribute types are skipped, as requested in the
      // |inspect_delegate.onInspectAttributeMeshpart()|.
    }
    return pmc::Error::OK;
  };

  // Decode from the PMC PMC buffer.
  ret = decoder.decode(buffer, decode_delegate);
  if (ret != pmc::Error::OK) {
    return ErrorStatus("PMC PMC decoding failed.");
  }

  // Check that mesh has only triangular faces.
  if (geom_info.faceType != pmc::MeshFaceType::TRIANGULAR) {
    return ErrorStatus("Only triangular faces are supported.");
  }

  // Reconstruct Draco mesh.
  // TODO(vytyaz): Use point cloud decoding to preserve input indices and add
  // faces at a later step for meshes with morph targets or joints.
  draco::TriangleSoupMeshBuilder mb;
  mb.Start(geom_info.faceCount);

  // Construct AttributeMeshpartInfo to process CoordinateSystem during
  // dequantization of position attribute.
  pmc::AttributeMeshpartInfo geom_attr_info;
  geom_attr_info.coordSys.emplace();
  geom_attr_info.coordSys->scale = geom_info.coordSys;
  geom_attr_info.coordSys->origin.assign(geom_info.coordSysOrigin,
                                         geom_info.coordSysOrigin + 3);
  geom_attr_info.coordSys->originScaleLog2.assign(
      geom_info.coordSysOriginScaleLog2, geom_info.coordSysOriginScaleLog2 + 3);

  // Populate positions.
  DRACO_ASSIGN_OR_RETURN(const int position_att_id,
                         (PopulateAttribute<float, 3>(
                             draco::GeometryAttribute::POSITION,
                             draco::DT_FLOAT32, 3, geom_attr_info, indices,
                             positions, geom_info.faceCount, false, &mb)));

  // Populate texture coordinate attributes.
  for (auto& tc : decoded_texcoords) {
    DRACO_ASSIGN_OR_RETURN(
        tc->att_id, (PopulateAttribute<float, 2>(
                        draco::GeometryAttribute::TEX_COORD, draco::DT_FLOAT32,
                        tc->info.componentsPerVector, tc->info, tc->indices,
                        tc->values, geom_info.faceCount, false, &mb)));
  }

  // Populate normal attribute.
  DRACO_ASSIGN_OR_RETURN(
      const int normal_att_id,
      (PopulateAttribute<float, 3>(
          draco::GeometryAttribute::NORMAL, draco::DT_FLOAT32,
          normal_info.componentsPerVector, normal_info, normal_indices,
          normal_values, geom_info.faceCount,
          normal_info.coordSysProjection == pmc::CoordSysProjection::OCTAHEDRAL,
          &mb)));

  // Populate tangent attribute.
  DRACO_ASSIGN_OR_RETURN(
      const int tangent_att_id,
      (PopulateAttribute<float, 4>(
          draco::GeometryAttribute::TANGENT, draco::DT_FLOAT32,
          tangent_info.componentsPerVector, tangent_info, tangent_indices,
          tangent_values, geom_info.faceCount, false, &mb)));

  // Add color attribute.
  // Data type is float32 if a coordinate system exists.
  const bool is_float_color = color_info.coordSys.has_value();
  if (color_info.componentsPerVector == 3) {
    if (is_float_color) {
      DRACO_RETURN_IF_ERROR((PopulateColorAttribute<float, 3>(
          color_info, color_indices, color_values, geom_info.faceCount, &mb)));
    } else {
      DRACO_RETURN_IF_ERROR((PopulateColorAttribute<uint8_t, 3>(
          color_info, color_indices, color_values, geom_info.faceCount, &mb)));
    }
  } else if (color_info.componentsPerVector == 4) {
    if (is_float_color) {
      DRACO_RETURN_IF_ERROR((PopulateColorAttribute<float, 4>(
          color_info, color_indices, color_values, geom_info.faceCount, &mb)));
    } else {
      DRACO_RETURN_IF_ERROR((PopulateColorAttribute<uint8_t, 4>(
          color_info, color_indices, color_values, geom_info.faceCount, &mb)));
    }
  }

  // Add weight attribute.
  DRACO_ASSIGN_OR_RETURN(
      const int weight_att_id,
      (PopulateAttribute<float, 4>(
          draco::GeometryAttribute::WEIGHTS, draco::DT_FLOAT32,
          weight_info.componentsPerVector, weight_info, weight_indices,
          weight_values, geom_info.faceCount, false, &mb)));

  // Add joint attribute.
  DRACO_ASSIGN_OR_RETURN(
      const int joint_att_id,
      (PopulateAttribute<uint16_t, 4>(
          draco::GeometryAttribute::JOINTS, draco::DT_UINT16,
          joint_info.componentsPerVector, joint_info, joint_indices,
          joint_values, geom_info.faceCount, false, &mb)));

  // Finalize Draco mesh.
  auto built_mesh = mb.Finalize();
  if (built_mesh == nullptr) {
    return ErrorStatus("Failed to finalize Draco mesh.");
  }
  mesh->Copy(*built_mesh);

  // Sets attribute unique ID and determines whether zero unique ID is taken.
  bool zero_uid_is_taken = false;
  auto set_attribute_uid = [&](const auto& att_info, int att_id) {
    if (att_id != -1) {
      mesh->attribute(att_id)->set_unique_id(att_info.attributeId);
      if (att_info.attributeId == 0) {
        zero_uid_is_taken = true;
      }
    }
  };

  // Set unique IDs for all attributes and determine if zero unique ID is taken.
  for (const auto& tc : decoded_texcoords) {
    set_attribute_uid(tc->info, tc->att_id);
  }
  set_attribute_uid(normal_info, normal_att_id);
  set_attribute_uid(tangent_info, tangent_att_id);
  const int color_att_id_tmp =
      mesh->GetNamedAttributeId(GeometryAttribute::COLOR);
  set_attribute_uid(color_info, color_att_id_tmp);
  set_attribute_uid(weight_info, weight_att_id);
  set_attribute_uid(joint_info, joint_att_id);

  // If any non-position attribute has unique ID 0, then position unique
  // attribute ID is set to the lowest available non-zero value.
  if (zero_uid_is_taken) {
    // Find the smallest available unique ID starting from 1.
    int lowest_available_uid = 1;
    while (mesh->GetAttributeByUniqueId(lowest_available_uid) != nullptr) {
      lowest_available_uid++;
    }
    mesh->attribute(position_att_id)->set_unique_id(lowest_available_uid);
  } else {
    mesh->attribute(position_att_id)->set_unique_id(0);
  }

  return OkStatus();
}

}  // namespace draco

#endif  // DRACO_PMC_SUPPORTED
