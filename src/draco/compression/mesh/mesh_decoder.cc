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
#include "draco/compression/mesh/mesh_decoder.h"

namespace draco {

MeshDecoder::MeshDecoder() : mesh_(nullptr) {}

Status MeshDecoder::DecodeStep1(const DecoderOptions &options,
                           DecoderBuffer *in_buffer, Mesh *out_mesh) {
  mesh_ = out_mesh;
  return PointCloudDecoder::DecodeStep1(options, in_buffer, out_mesh);
}

Status MeshDecoder::DecodeStep2() {
    return PointCloudDecoder::DecodeStep2();
}

bool MeshDecoder::DecodeGeometryData() {
  if (mesh_ == nullptr) {
    return false;
  }
  if (!DecodeConnectivity()) {
    return false;
  }
  return PointCloudDecoder::DecodeGeometryData();
}

}  // namespace draco
