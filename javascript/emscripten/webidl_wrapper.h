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
#ifndef DRACO_JAVASCRIPT_EMSCRITPEN_WEBIDL_WRAPPER_H_
#define DRACO_JAVASCRIPT_EMSCRITPEN_WEBIDL_WRAPPER_H_

#include <vector>

#include "compression/config/compression_shared.h"
#include "core/decoder_buffer.h"
#include "mesh/mesh.h"
#include "point_cloud/point_attribute.h"

typedef draco::GeometryAttribute draco_GeometryAttribute;
typedef draco_GeometryAttribute::Type draco_GeometryAttribute_Type;
typedef draco::EncodedGeometryType draco_EncodedGeometryType;

// To generate Draco JabvaScript bindings you must have emscripten installed.
// Then run make -f Makefile.emcc jslib.
namespace draco {

class DracoFloat32Array {
 public:
  DracoFloat32Array();
  float GetValue(int index) const;

  // In case |values| is nullptr, the data is allocated but not initialized.
  bool SetValues(const float *values, int count);

  // Directly sets a value for a specific index. The array has to be already
  // allocated at this point (using SetValues() method).
  void SetValue(int index, float val) { values_[index] = val; }

 private:
  std::vector<float> values_;
};

class DracoInt32Array {
 public:
  DracoInt32Array();
  int GetValue(int index) const;
  bool SetValues(const int *values, int count);

 private:
  std::vector<int> values_;
};

// Class used by emscripten WebIDL Binder [1] to wrap calls to decode Draco
// data.
// [1]http://kripken.github.io/emscripten-site/docs/porting/connecting_cpp_and_javascript/WebIDL-Binder.html
class WebIDLWrapper {
 public:
  WebIDLWrapper();

  // Returns the geometry type stored in the |in_buffer|. Return values can be
  // INVALID_GEOMETRY_TYPE, POINT_CLOUD, or MESH.
  draco_EncodedGeometryType GetEncodedGeometryType(DecoderBuffer *in_buffer);

  // Decodes a point cloud from the provided buffer. The caller is responsible
  // for deleting the PointCloud pointer.
  static PointCloud *DecodePointCloudFromBuffer(DecoderBuffer *in_buffer);

  // Decodes a triangular mesh from the provided buffer. The caller is
  // responsible for deleting the Mesh pointer.
  static Mesh *DecodeMeshFromBuffer(DecoderBuffer *in_buffer);

  // Returns an attribute id for the first attribute of a given type.
  long GetAttributeId(const PointCloud &pc, draco_GeometryAttribute_Type type);

  // Returns a PointAttribute pointer from |att_id| index.
  static const PointAttribute *GetAttribute(const PointCloud &pc, long att_id);

  // Returns Mesh::Face values in |out_values| from |face_id| index.
  static bool GetFaceFromMesh(const Mesh &m, FaceIndex::ValueType face_id,
                              DracoInt32Array *out_values);
  // Returns float attribute values in |out_values| from |entry_index| index.
  static bool GetAttributeFloat(const PointAttribute &pa,
                                AttributeValueIndex::ValueType entry_index,
                                DracoFloat32Array *out_values);

  // Returns float attribute values for all point ids of the point cloud.
  // I.e., the |out_values| is going to contain m.num_points() entries.
  static bool GetAttributeFloatForAllPoints(const PointCloud &pc,
                                            const PointAttribute &pa,
                                            DracoFloat32Array *out_values);
};

}  // namespace draco

#endif  // DRACO_JAVASCRIPT_EMSCRITPEN_WEBIDL_WRAPPER_H_
