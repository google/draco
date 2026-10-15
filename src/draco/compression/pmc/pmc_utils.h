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
#ifndef DRACO_COMPRESSION_PMC_PMC_UTILS_H_
#define DRACO_COMPRESSION_PMC_PMC_UTILS_H_

#include "draco/draco_features.h"

#ifdef DRACO_PMC_SUPPORTED

#include <cstddef>
#include <cstdint>
#include <limits>
#include <pmc/pmCodec.hpp>
#include <pmc/pmEncoder.hpp>
#include <pmc/pmTypesAndConstants.hpp>
#include <sstream>
#include <string>
#include <vector>

#include "draco/attributes/geometry_indices.h"
#include "draco/attributes/point_attribute.h"
#include "draco/core/bounding_box.h"
#include "draco/core/vector_d.h"

namespace draco {

// Class containing utility functions related to PMC PMC encoding and decoding.
class PmcUtils {
 public:
  // Creates a PMC PMC array buffer from the given data.
  static pmc::ArrayBuffer CreateArrayBuffer(std::vector<int32_t>& data,
                                            size_t components_per_vector);

  // Creates a PMC PMC geometry meshpart from the given data.
  static pmc::GeometryMeshpart CreateGeometryMeshpart(
      std::vector<int32_t>& positions, std::vector<int32_t>& face_degrees,
      std::vector<int32_t>& indices, int32_t bit_depth);

  // Creates a PMC PMC attribute meshpart from the given data.
  static pmc::AttributeMeshpart CreateAttributeMeshpart(
      std::vector<int32_t>& values, std::vector<int32_t>& indices,
      int32_t att_id, int32_t components_per_vector,
      pmc::AttributeType att_type, pmc::AttributeScope att_scope,
      int32_t bitdepth, bool octahedral_normals = false);

  // Creates PMC PMC geometry info.
  static pmc::GeometryInfo CreateGeometryInfo(int32_t bit_depth,
                                              const Vector3f& min_vals,
                                              float scale);

  // Approximates floating point value as rational number.
  static pmc::Rational FloatToRational(float value);

  // Creates PMC PMC attribute info from attribute meshpart info.
  template <typename DataTypeT, int dimension_t>
  static pmc::AttributeInfo CreateAttributeInfo(
      const pmc::AttributeMeshpartInfo& meshpart_info,
      const VectorD<DataTypeT, dimension_t>& min_vals, float scale,
      const std::string& name) {
    pmc::AttributeInfo info;
    info.attributeId = meshpart_info.attributeId;
    info.coordSysProjection = meshpart_info.coordSysProjection;
    info.componentsPerVector = meshpart_info.componentsPerVector;
    info.type = meshpart_info.type;
    info.scope = meshpart_info.scope;
    info.name = name;

    if (scale != 0.0f) {
      pmc::AttributeInfo::CoordinateSystem cs;
      cs.scale = FloatToRational(scale);
      cs.origin.resize(dimension_t);
      cs.originScaleLog2.resize(dimension_t, 0);
      for (int i = 0; i < dimension_t; ++i) {
        cs.origin[i] = static_cast<int32_t>(std::round(min_vals[i] * scale));
      }
      info.coordSys = cs;
    } else {
      info.coordSys = std::nullopt;
    }
    return info;
  }

  // Computes bounding box for a given attribute.
  template <typename T, int D>
  static BoundingBoxD<T, D> ComputeBoundingBox(
      const draco::PointAttribute& att) {
    BoundingBoxD<T, D> bbox;
    draco::VectorD<T, D> value;
    for (draco::AttributeValueIndex avi(0); avi < att.size(); ++avi) {
      att.GetValue(avi, value.data());
      bbox.Update(value);
    }
    return bbox;
  }

  // Computes scale for a given bounding box and quantization bits.
  template <typename BBoxT>
  static float ComputeQuantizationScale(const BBoxT& bbox,
                                        int quantization_bits) {
    float max_delta = bbox.Size().MaxCoeff();
    if (max_delta < std::numeric_limits<float>::epsilon()) {
      max_delta = 1.0f;
    }
    return (static_cast<float>((1 << quantization_bits) - 1)) / max_delta;
  }

  // Creates PMC PMC geometry encoding parameters.
  static pmc::GeometryEncodingParameters CreateGeometryEncodingParameters();

  // Creates PMC PMC attribute encoding parameters for normals.
  static pmc::AttributeEncodingParameters
  CreateNormalAttributeEncodingParameters();

  // Creates PMC PMC attribute encoding parameters for texture coordinates.
  static pmc::AttributeEncodingParameters
  CreateTexCoordAttributeEncodingParameters();

  // Creates PMC PMC attribute encoding parameters for generic attributes.
  static pmc::AttributeEncodingParameters
  CreateGenericAttributeEncodingParameters();

  // Quantizes coordinate.
  static int32_t Quantize(float value, float min_val, float scale);

  // Dequantizes coordinate.
  static float Dequantize(int32_t value, float min_val, float scale);

  // Quantizes normal vector using octahedral quantization.
  static void QuantizeOctahedralNormal(const float* value, int32_t qbits,
                                       int32_t* quantized_value);

  // Dequantizes normal vector using octahedral quantization.
  static void DequantizeOctahedralNormal(const int32_t* value,
                                         float* dequantized_value);
};

}  // namespace draco

#endif  // DRACO_PMC_SUPPORTED
#endif  // DRACO_COMPRESSION_PMC_PMC_UTILS_H_
