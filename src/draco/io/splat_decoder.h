// Copyright 2025 Patrick Trollmann.
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
#ifndef DRACO_IO_SPLAT_DECODER_H_
#define DRACO_IO_SPLAT_DECODER_H_

#include <fstream>
#include <vector>

#include "draco/point_cloud/point_cloud.h"
#include "draco/core/status.h"

namespace draco {

// Decodes a SPLAT file into draco::PointCloud.
// The current implementation assumes the properties: POSITION, SCALE, COLOR, ROTATION.
// POSITION and SCALE are stored together as GeometryAttribute.POSITION. COLOR and ROATION
// are stored together as GeometryAttribute.COLOR. Currently, it is not possible to use 
// custom GeometryAttribute types or the GENERIC type. This types cannot be decoded in
// the WebGL version of Draco.
class SplatDecoder {
 public:
  SplatDecoder();

  // Decodes a splat file stored in the input file.
  Status ReadSplatFile(const std::string &filename,
                       PointCloud *out_point_cloud);

};

}  // namespace draco

#endif  // DRACO_IO_SPLAT_DECODER_H_
