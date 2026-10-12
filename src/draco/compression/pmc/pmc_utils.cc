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
#include "draco/compression/pmc/pmc_utils.h"

#ifdef DRACO_PMC_SUPPORTED

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <pmc/pmCodec.hpp>
#include <pmc/pmEncoder.hpp>
#include <pmc/pmTypesAndConstants.hpp>
#include <string>
#include <vector>

#include "draco/core/vector_d.h"

namespace draco {

pmc::ArrayBuffer PmcUtils::CreateArrayBuffer(std::vector<int32_t>& data,
                                             size_t components_per_vector) {
  pmc::ArrayBuffer buf;
  buf.data = reinterpret_cast<uint8_t*>(data.data());
  buf.dataType = pmc::DataType::Int32;
  buf.offset = 0;
  buf.componentsPerVector = components_per_vector;
  buf.vectorCount = data.size() / components_per_vector;
  buf.stride = components_per_vector * sizeof(int32_t);
  return buf;
}

pmc::GeometryMeshpart PmcUtils::CreateGeometryMeshpart(
    std::vector<int32_t>& positions, std::vector<int32_t>& face_degrees,
    std::vector<int32_t>& indices, int32_t bit_depth) {
  pmc::GeometryMeshpartInfo info;
  info.vertexCount = positions.size() / 3;
  info.faceCount = face_degrees.size();
  info.indexCount = indices.size();
  info.faceType = pmc::MeshFaceType::TRIANGULAR;
  info.coordSys = pmc::Rational{(1 << bit_depth) - 1, 1};
  info.connectivityRegularized = true;

  pmc::GeometryMeshpartBuffers buffers;
  buffers.positions = PmcUtils::CreateArrayBuffer(positions, 3);
  buffers.faceDegrees = PmcUtils::CreateArrayBuffer(face_degrees, 1);
  buffers.indices = PmcUtils::CreateArrayBuffer(indices, 1);

  pmc::GeometryMeshpart meshpart;
  meshpart.info = info;
  meshpart.buffers = buffers;
  return meshpart;
}

pmc::AttributeMeshpart PmcUtils::CreateAttributeMeshpart(
    std::vector<int32_t>& values, std::vector<int32_t>& indices, int32_t att_id,
    int32_t components_per_vector, pmc::AttributeType att_type,
    pmc::AttributeScope att_scope, int32_t bitdepth, bool octahedral_normals) {
  pmc::AttributeMeshpartInfo info;
  info.vectorCount = values.size() / components_per_vector;
  info.attributeId = att_id;
  info.componentsPerVector = components_per_vector;
  info.type = att_type;
  info.scope = att_scope;
  info.indexCount = indices.size();  // Can be zero for implicit indices.
  info.explicitIndices = !indices.empty();
  info.outputVectorCount = info.vectorCount;
  info.outputIndexCount = info.indexCount;
  info.coordSysProjection = octahedral_normals
                                ? pmc::CoordSysProjection::OCTAHEDRAL
                                : pmc::CoordSysProjection::IDENTITY;
  info.useTexCoordinatesPredictors =
      (att_type == pmc::AttributeType::TEX_COORD);
  pmc::AttributeInfo::CoordinateSystem cs;
  if (octahedral_normals) {
    cs.scale = pmc::Rational{(1 << (bitdepth - 1)) - 1, 1};
  } else {
    cs.scale = pmc::Rational{(1 << bitdepth) - 1, 1};
  }
  cs.origin.resize(components_per_vector, 0);
  cs.originScaleLog2.resize(components_per_vector, 0);
  info.coordSys = cs;

  pmc::AttributeMeshpartBuffers buffers;
  buffers.values = PmcUtils::CreateArrayBuffer(values, components_per_vector);
  buffers.indices = indices.empty() ? pmc::ArrayBuffer()
                                    : PmcUtils::CreateArrayBuffer(indices, 1);

  pmc::AttributeMeshpart meshpart;
  meshpart.info = info;
  meshpart.buffers = buffers;
  return meshpart;
}

pmc::Rational PmcUtils::FloatToRational(float value) {
  if (value <= 0.0f) {
    return pmc::Rational{0, 1};
  }
  double x = value;
  double tolerance = 1e-7;
  int32_t max_denominator = 1000000;

  int32_t m[2][2];
  m[0][0] = 1;
  m[0][1] = 0;
  m[1][0] = 0;
  m[1][1] = 1;

  int32_t p = 0, q = 1;

  for (int i = 0; i < 100; ++i) {
    int32_t a = static_cast<int32_t>(std::floor(x));
    int32_t next_p = a * m[0][0] + m[0][1];
    int32_t next_q = a * m[1][0] + m[1][1];

    if (next_q > max_denominator || next_p < 0) {
      break;
    }

    m[0][1] = m[0][0];
    m[0][0] = next_p;
    m[1][1] = m[1][0];
    m[1][0] = next_q;

    p = next_p;
    q = next_q;

    if (std::abs(x - a) < tolerance) {
      break;
    }
    x = 1.0 / (x - a);
  }

  return pmc::Rational{p, q};
}

pmc::GeometryInfo PmcUtils::CreateGeometryInfo(int32_t bit_depth,
                                               const Vector3f& min_vals,
                                               float scale) {
  pmc::GeometryInfo info;
  info.coordSys = FloatToRational(scale);
  for (int i = 0; i < 3; ++i) {
    info.coordSysOrigin[i] =
        static_cast<int32_t>(std::round(min_vals[i] * scale));
    info.coordSysOriginScaleLog2[i] = 0;
  }
  return info;
}

pmc::GeometryEncodingParameters PmcUtils::CreateGeometryEncodingParameters() {
  // TODO(vytyaz): Revisit configuration and make it customizable.
  pmc::GeometryEncodingParameters params;
  params.connReg.mode = pmc::ConnectivityRegularizationMode::ADAPTIVE;
  params.predictionStrategy = pmc::PredictionStrategy::LINEAR;
  params.deduplicateVertices = true;
  params.deduplicateVerticesSimple = false;
  params.deduplicationStrategy = pmc::DeduplicationStrategy::UNQUANTIZED;
  params.flipSign = 2;
  params.groupCtx = 2;
  params.multiwayPredictorRunningWeightsLog2UpdatePeriod = 9;
  params.multiwayPredictionLog2UpdatePeriod = 8;
  params.predictionSnapToPlane = 2;
  params.sortingStrategy = pmc::SortingStrategy::ADAPTIVE;
  params.singlewayPredictorRunningWeightsLog2UpdatePeriod = 6;
  params.singlewayPredictionLog2UpdatePeriod = 8;
  params.traversalStrategy = pmc::TraversalStrategy::GUIDED_ADAPTIVE;
  params.prevHighOutlierResidual = 2;
  params.sameHighOutlierResidual = 2;
  params.deltaCodeResidual = 2;
  return params;
}

pmc::AttributeEncodingParameters
PmcUtils::CreateNormalAttributeEncodingParameters() {
  // TODO(vytyaz): Revisit configuration and make it customizable.
  pmc::AttributeEncodingParameters params;
  params.deduplicationStrategy = pmc::DeduplicationStrategy::UNQUANTIZED;
  params.deduplicateValuesBitfield = 0;
  params.indicesCodingStrategy = pmc::AttributeIndicesCodingStrategy::ADAPTIVE;
  params.multiwayPredictionLog2UpdatePeriod = 8;
  params.predictionStrategy =
      pmc::PredictionStrategy::UNITARY_OCTAHEDRAL_NORMAL_VECTOR;
  params.singlewayPredictionLog2UpdatePeriod = 8;
  params.traversalStrategy = pmc::TraversalStrategy::GEOMETRY_DEFINED;
  params.multiwayPredictorRunningWeightsLog2UpdatePeriod = 10;
  params.prevHighOutlierResidual = 2;
  params.sameHighOutlierResidual = 2;
  params.deltaCodeResidual = 2;
  return params;
}

pmc::AttributeEncodingParameters
PmcUtils::CreateTexCoordAttributeEncodingParameters() {
  // TODO(vytyaz): Revisit configuration and make it customizable.
  pmc::AttributeEncodingParameters params;
  params.indicesCodingStrategy = pmc::AttributeIndicesCodingStrategy::ADAPTIVE;
  params.multiwayPredictionLog2UpdatePeriod = 8;
  params.predictionStrategy =
      pmc::PredictionStrategy::TEX_COORD_GEOMETRY_GUIDED;
  params.singlewayPredictionLog2UpdatePeriod = 8;
  params.traversalStrategy = pmc::TraversalStrategy::CONNECTIVITY_GUIDED;
  params.prevHighOutlierResidual = 2;
  params.sameHighOutlierResidual = 2;
  params.deltaCodeResidual = 2;
  return params;
}

pmc::AttributeEncodingParameters
PmcUtils::CreateGenericAttributeEncodingParameters() {
  // TODO(vytyaz): Revisit configuration and make it customizable.
  pmc::AttributeEncodingParameters params;
  params.deduplicationStrategy = pmc::DeduplicationStrategy::DEFAULT;
  params.deduplicateValuesBitfield = 0;
  params.indicesCodingStrategy = pmc::AttributeIndicesCodingStrategy::ADAPTIVE;
  params.multiwayPredictionLog2UpdatePeriod = 8;
  params.predictionStrategy = pmc::PredictionStrategy::LINEAR;
  params.singlewayPredictionLog2UpdatePeriod = 8;
  params.traversalStrategy = pmc::TraversalStrategy::GEOMETRY_DEFINED;
  params.multiwayPredictorRunningWeightsLog2UpdatePeriod = 10;
  params.prevHighOutlierResidual = 2;
  params.sameHighOutlierResidual = 2;
  params.deltaCodeResidual = 2;
  return params;
}

int32_t PmcUtils::Quantize(float value, float min_val, float scale) {
  return static_cast<int32_t>(std::round((value - min_val) * scale));
}

float PmcUtils::Dequantize(int32_t value, float min_val, float scale) {
  return static_cast<float>(value) / scale + min_val;
}

void PmcUtils::QuantizeOctahedralNormal(const float* value, int32_t qbits,
                                        int32_t* quantized_value) {
  // The value 0 is represented by the integer value 2^(qbits - 1) - 1.
  const float halfMax = static_cast<float>((1 << qbits >> 1) - 1);
  float sum = 0;
  for (int k = 0; k < 3; ++k) {
    sum += std::abs(value[k]);
  }
  if (sum == 0.0f) {
    // Zero normal vector.
    sum = 1.0f;
  }
  float scaledVec[3];
  int32_t sumInt = 0;
  for (int k = 0; k < 3; ++k) {
    // Normalize |value| in terms of L1-norm, then scale it by |halfMax|.
    scaledVec[k] = value[k] / sum * halfMax;
    // Using std::trunc instead of std::round to make sure that
    // |halfMax| - 3 <= |sumInt| <= |halfMax| when exiting this loop.
    quantized_value[k] = static_cast<int32_t>(std::trunc(scaledVec[k]));
    sumInt += std::abs(quantized_value[k]);
  }
  if (halfMax - sumInt > 1) {
    for (int k = 0; k < 3; ++k) {
      sumInt -= std::abs(quantized_value[k]);
      quantized_value[k] += scaledVec[k] < 0 ? -1 : 1;
      sumInt += std::abs(quantized_value[k]);
    }
  }
  if (sumInt != static_cast<int32_t>(halfMax)) {
    // Make sure |sumInt| == |halfMax|.
    int err = static_cast<int32_t>(halfMax) - sumInt;
    int bl = -1;
    double ba = std::numeric_limits<double>::max();
    for (int l = 0; l < 3; ++l) {
      double dot = 0;
      double n_a = 0;
      double val[3];
      for (int k = 0; k < 3; ++k) {
        val[k] =
            quantized_value[k] + (k == l ? (scaledVec[k] < 0 ? -err : err) : 0);
        dot += scaledVec[k] * val[k];
        n_a += scaledVec[k] * scaledVec[k];
      }
      double w = dot > 0.0 ? n_a / dot : 1.0;
      double dist = 0.0;
      for (int k = 0; k < 3; ++k) {
        double d = scaledVec[k] - w * val[k];
        dist += d * d;
      }
      if (dist < ba) {
        ba = dist;
        bl = l;
      }
    }
    quantized_value[bl] += scaledVec[bl] < 0 ? -err : err;
  }
}

void PmcUtils::DequantizeOctahedralNormal(const int32_t* value,
                                          float* dequantized_value) {
  double l2_norm_sq = 0;
  for (int k = 0; k < 3; ++k) {
    const float val = static_cast<float>(value[k]);
    dequantized_value[k] = val;
    l2_norm_sq += static_cast<double>(val) * static_cast<double>(val);
  }
  const double l2_norm = std::sqrt(l2_norm_sq);
  for (int k = 0; k < 3; ++k) {
    dequantized_value[k] = static_cast<float>(dequantized_value[k] / l2_norm);
  }
}

}  // namespace draco

#endif  // DRACO_PMC_SUPPORTED
