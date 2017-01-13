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
#ifndef DRACO_COMPRESSION_ENCODE_H_
#define DRACO_COMPRESSION_ENCODE_H_

#include "compression/config/compression_shared.h"
#include "compression/config/encoder_options.h"
#include "core/encoder_buffer.h"
#include "mesh/mesh.h"

namespace draco {

// Encodes point cloud to the provided buffer. |options| can be used to control
// the encoding of point cloud. See functions below that can be used to generate
// valid options for the encoder covering the most usual use cases.
bool EncodePointCloudToBuffer(const PointCloud &pc,
                              const EncoderOptions &options,
                              EncoderBuffer *out_buffer);

// Encodes mesh to the provided buffer.
bool EncodeMeshToBuffer(const Mesh &m, const EncoderOptions &options,
                        EncoderBuffer *out_buffer);

// Creates default encoding options that contain a valid set of features that
// the encoder can use. Otherwise all options are left unitialized which results
// in a lossless compression.
EncoderOptions CreateDefaultEncoderOptions();

// Sets the desired encoding and decoding speed for the given options.
//
//  0 = slowest speed, but the best compression.
// 10 = fastest, but the worst compression.
// -1 = undefined.
//
// Note that both speed options affect the encoder choice of used methods and
// algorithms. For example, a requirement for fast decoding may prevent the
// encoder from using the best compression methods even if the encoding speed is
// set to 0. In general, the faster of the two options limits the choice of
// features that can be used by the encoder. Additionally, setting
// |decoding_speed| to be faster than the |encoding_speed| may allow the encoder
// to choose the optimal method out of the available features for the given
// |decoding_speed|.
void SetSpeedOptions(EncoderOptions *options, int encoding_speed,
                     int decoding_speed);

// Sets the quantization compression options for a named attribute. The
// attribute values will be quantized in a box defined by the maximum extent of
// the attribute values. I.e., the actual precision of this option depends on
// the scale of the attribute values.
void SetNamedAttributeQuantization(EncoderOptions *options,
                                   const PointCloud &pc,
                                   GeometryAttribute::Type type,
                                   int quantization_bits);

// Sets the above quantization directly for a specific attribute |options|.
void SetAttributeQuantization(Options *options, int quantization_bits);

// Enables/disables built in entropy coding of attribute values. Disabling this
// option may be useful to improve the performance when third party compression
// is used on top of the Draco compression.
// Default: [true].
void SetUseBuiltInAttributeCompression(EncoderOptions *options, bool enabled);

// Sets the desired encoding method for a given geometry. By default, encoding
// method is selected based on the properties of the input geometry and based on
// the other options selected in the used EncoderOptions (such as desired
// encoding and decoding speed). This function should be called only when a
// specific method is required.
//
// |encoding_method| can be one of the following as defined in
// compression/config/compression_shared.h :
//   POINT_CLOUD_SEQUENTIAL_ENCODING
//   POINT_CLOUD_KD_TREE_ENCODING
//   MESH_SEQUENTIAL_ENCODING
//   MESH_EDGEBREAKER_ENCODING
//
// If the selected method cannot be used for the given input, the subsequent
// call of EncodePointCloudToBuffer or EncodeMeshToBuffer is going to fail.
void SetEncodingMethod(EncoderOptions *options, int encoding_method);

// Sets the desired prediction method for a given attribute. By default,
// prediction scheme is selected automatically by the encoder using other
// provided options (such as speed) and input geometry type (mesh, point cloud).
// This function should be called only when a specific prediction is prefered
// (e.g., when it is known that the encoder would select a less optimal
// prediction for the given input data).
//
// |prediction_scheme_method| should be one of the entries defined in
// compression/config/compression_shared.h :
//
//   PREDICTION_NONE - use no prediction.
//   PREDICTION_DIFFERENCE - delta coding
//   MESH_PREDICTION_PARALLELOGRAM - parallelogram prediction for meshes.
//   MESH_PREDICTION_MULTI_PARALLELOGRAM
//      - better and more costly version of the parallelogram prediction.
//   MESH_PREDICTION_TEX_COORDS - specialized predictor for tex coordinates.
//
// Note that in case the desired prediction cannot be used, the default
// prediction will be automatically used instead.
void SetNamedAttributePredictionScheme(EncoderOptions *options,
                                       const PointCloud &pc,
                                       GeometryAttribute::Type type,
                                       int prediction_scheme_method);

// Sets the prediction scheme directly for a specific attribute |options|.
void SetAttributePredictionScheme(Options *options,
                                  int prediction_scheme_method);

}  // namespace draco

#endif  // DRACO_COMPRESSION_ENCODE_H_
