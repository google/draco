// Copyright 2016 The Draco Authors.
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
#include "compression/encode.h"

#include "compression/mesh/mesh_edgebreaker_encoder.h"
#include "compression/mesh/mesh_sequential_encoder.h"
#include "compression/point_cloud/point_cloud_kd_tree_encoder.h"
#include "compression/point_cloud/point_cloud_sequential_encoder.h"

namespace draco {

// Encodes header common to all methods.
bool EncodeHeader(const PointCloudEncoder &encoder, EncoderBuffer *out_buffer) {
  // Encode the header according to our v1 specification.
  // Five bytes for Draco format.
  out_buffer->Encode("DRACO", 5);
  // Version (major, minor).
  const uint8_t major_version = 1;
  const uint8_t minor_version = 1;
  out_buffer->Encode(major_version);
  out_buffer->Encode(minor_version);
  // Type of the encoder (point cloud, mesh, ...).
  const uint8_t encoder_type = encoder.GetGeometryType();
  out_buffer->Encode(encoder_type);
  // Unique identifier for the selected encoding method (edgebreaker, etc...).
  out_buffer->Encode(encoder.GetEncodingMethod());
  // Reserved for flags.
  out_buffer->Encode(static_cast<uint16_t>(0));
  return true;
}

bool EncodeGeometryToBuffer(PointCloudEncoder *encoder,
                            const EncoderOptions &options,
                            EncoderBuffer *out_buffer) {
  if (!encoder)
    return false;
  if (!EncodeHeader(*encoder, out_buffer))
    return false;
  if (!encoder->Encode(options, out_buffer))
    return false;
  return true;
}

bool EncodePointCloudToBuffer(const PointCloud &pc,
                              const EncoderOptions &options,
                              EncoderBuffer *out_buffer) {
  std::unique_ptr<PointCloudEncoder> encoder;
  if (options.GetSpeed() < 10 && pc.num_attributes() == 1) {
    const PointAttribute *const att = pc.attribute(0);
    bool create_kd_tree_encoder = true;
    // Kd-Tree encoder can be currently used only under following conditions:
    //   - Point cloud has one attribute describing positions
    //   - Position is described by three components (x,y,z)
    //   - Position data type is one of the following:
    //         -float32 and quantization is enabled
    //         -uint32
    if (att->attribute_type() != GeometryAttribute::POSITION ||
        att->components_count() != 3)
      create_kd_tree_encoder = false;
    if (create_kd_tree_encoder && att->data_type() != DT_FLOAT32 &&
        att->data_type() != DT_UINT32)
      create_kd_tree_encoder = false;
    if (create_kd_tree_encoder && att->data_type() == DT_FLOAT32 &&
        options.GetAttributeInt(0, "quantization_bits", -1) <= 0)
      create_kd_tree_encoder = false;  // Quantization not enabled.
    if (create_kd_tree_encoder) {
      // Create kD-tree encoder.
      encoder =
          std::unique_ptr<PointCloudEncoder>(new PointCloudKdTreeEncoder());
    }
  }
  if (!encoder) {
    // Default choice.
    encoder =
        std::unique_ptr<PointCloudEncoder>(new PointCloudSequentialEncoder());
  }
  if (encoder)
    encoder->SetPointCloud(pc);
  return EncodeGeometryToBuffer(encoder.get(), options, out_buffer);
}

bool EncodeMeshToBuffer(const Mesh &m, const EncoderOptions &options,
                        EncoderBuffer *out_buffer) {
  std::unique_ptr<MeshEncoder> encoder;
  // Select the encoding method only based on the provided options.
  int encoding_method = options.GetGlobalInt("encoding_method", -1);
  if (encoding_method == -1) {
    // For now select the edgebreaker for all options expect of speed 10
    if (options.GetSpeed() == 10) {
      encoding_method = MESH_SEQUENTIAL_ENCODING;
    } else {
      encoding_method = MESH_EDGEBREAKER_ENCODING;
    }
  }
  if (encoding_method == MESH_EDGEBREAKER_ENCODING) {
    encoder = std::unique_ptr<MeshEncoder>(new MeshEdgeBreakerEncoder());
  } else {
    encoder = std::unique_ptr<MeshEncoder>(new MeshSequentialEncoder());
  }
  if (encoder)
    encoder->SetMesh(m);
  return EncodeGeometryToBuffer(encoder.get(), options, out_buffer);
}

EncoderOptions CreateDefaultEncoderOptions() {
  return EncoderOptions::CreateDefaultOptions();
}

void SetSpeedOptions(EncoderOptions *options, int encoding_speed,
                     int decoding_speed) {
  options->GetGlobalOptions()->SetInt("encoding_speed", encoding_speed);
  options->GetGlobalOptions()->SetInt("decoding_speed", decoding_speed);
}

void SetNamedAttributeQuantization(EncoderOptions *options,
                                   const PointCloud &pc,
                                   GeometryAttribute::Type type,
                                   int quantization_bits) {
  Options *const o = options->GetNamedAttributeOptions(pc, type);
  if (o) {
    SetAttributeQuantization(o, quantization_bits);
  }
}

void SetAttributeQuantization(Options *options, int quantization_bits) {
  options->SetInt("quantization_bits", quantization_bits);
}

void SetUseBuiltInAttributeCompression(EncoderOptions *options, bool enabled) {
  options->GetGlobalOptions()->SetBool("use_built_in_attribute_compression",
                                       enabled);
}

void SetNamedAttributePredictionScheme(EncoderOptions *options,
                                       const PointCloud &pc,
                                       GeometryAttribute::Type type,
                                       int prediction_scheme_method) {
  Options *const o = options->GetNamedAttributeOptions(pc, type);
  if (o) {
    SetAttributePredictionScheme(o, prediction_scheme_method);
  }
}

void SetAttributePredictionScheme(Options *options,
                                  int prediction_scheme_method) {
  options->SetInt("prediction_scheme", prediction_scheme_method);
}

}  // namespace draco
