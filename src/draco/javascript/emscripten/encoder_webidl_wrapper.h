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
#ifndef DRACO_JAVASCRIPT_EMSCRITPEN_ENCODER_WEBIDL_WRAPPER_H_
#define DRACO_JAVASCRIPT_EMSCRITPEN_ENCODER_WEBIDL_WRAPPER_H_

#include <vector>

#include "draco/attributes/point_attribute.h"
#include "draco/compression/config/compression_shared.h"
#include "draco/compression/config/encoder_options.h"
#include "draco/compression/encode.h"
#include "draco/mesh/mesh.h"

typedef draco::GeometryAttribute draco_GeometryAttribute;
typedef draco::GeometryAttribute::Type draco_GeometryAttribute_Type;
typedef draco::EncodedGeometryType draco_EncodedGeometryType;
typedef draco::MeshEncoderMethod draco_MeshEncoderMethod;

class DracoFloat32Array {
 public:
  DracoFloat32Array();
  float GetValue(int index) const;

  // In case |values| is nullptr, the data is allocated but not initialized.
  bool SetValues(const float *values, int count);

  // Directly sets a value for a specific index. The array has to be already
  // allocated at this point (using SetValues() method).
  void SetValue(int index, float val) { values_[index] = val; }

  size_t size() { return values_.size(); }

 private:
  std::vector<float> values_;
};

class DracoInt8Array {
 public:
  DracoInt8Array();
  int GetValue(int index) const;
  bool SetValues(const char *values, int count);

  size_t size() { return values_.size(); }

 private:
  std::vector<int> values_;
};

class MetadataBuilder {
 public:
  MetadataBuilder();
  bool AddStringEntry(draco::Metadata *metadata, const char *entry_name,
                      const char *entry_value);
  bool AddIntEntry(draco::Metadata *metadata, const char *entry_name,
                   long entry_value);
  bool AddDoubleEntry(draco::Metadata *metadata, const char *entry_name,
                      double entry_value);
};

// TODO(zhafang): Regenerate wasm decoder.
// TODO(zhafang): Add script to generate and test all Javascipt code.
class MeshBuilder {
 public:
  MeshBuilder();

  bool AddFacesToMesh(draco::Mesh *mesh, long num_faces, const int *faces);
  int AddFloatAttributeToMesh(draco::Mesh *mesh,
                              draco_GeometryAttribute_Type type,
                              long num_vertices, long num_components,
                              const float *att_values);
  bool SetMetadataForAttribute(draco::Mesh *mesh, long attribute_id,
                               const draco::Metadata *metadata);
  bool AddMetadataToMesh(draco::Mesh *mesh, const draco::Metadata *metadata);
};

class Encoder {
 public:
  Encoder();

  void SetEncodingMethod(draco_MeshEncoderMethod method);
  void SetAttributeQuantization(draco_GeometryAttribute_Type type,
                                long quantization_bits);
  void SetAttributeExplicitQuantization(draco_GeometryAttribute_Type type,
                                        long quantization_bits,
                                        long num_components,
                                        const float *origin, float range);
  void SetSpeedOptions(long encoding_speed, long decoding_speed);
  int EncodeMeshToDracoBuffer(draco::Mesh *mesh, DracoInt8Array *buffer);

 private:
  draco::Encoder encoder_;
};

#endif  // DRACO_JAVASCRIPT_EMSCRITPEN_ENCODER_WEBIDL_WRAPPER_H_
