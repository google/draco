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
#include "draco/io/splat_decoder.h"

namespace draco {
SplatDecoder::SplatDecoder() {}

Status SplatDecoder::ReadSplatFile(const std::string &filename,
                         PointCloud *out_point_cloud) {
  std::ifstream file(filename, std::ios::binary);
  if (!file) {
    return Status(Status::DRACO_ERROR, "Unable to open input file.");
  }

  struct Gaussian {
    float position[3];
    float scale[3];
    uint8_t color[4];
    uint8_t rotation[4];
  };

  std::vector<Gaussian> gaussians;
  Gaussian gaussian;
  while (file.read(reinterpret_cast<char *>(&gaussian), sizeof(Gaussian))) {
    gaussians.push_back(gaussian);
  }
  const PointIndex::ValueType num_vertices = gaussians.size();
  out_point_cloud->set_num_points(num_vertices);

  GeometryAttribute vapos;
  vapos.Init(GeometryAttribute::POSITION, nullptr, 6, DT_FLOAT32, false,
             sizeof(float) * 6, 0);
  const int att_id_position =
      out_point_cloud->AddAttribute(vapos, true, num_vertices);

  GeometryAttribute vacol;
  vacol.Init(GeometryAttribute::COLOR, nullptr, 8, DT_UINT8, true,
             sizeof(uint8_t) * 8, 0);
  const int att_id_color =
      out_point_cloud->AddAttribute(vacol, true, num_vertices);

  for (PointIndex::ValueType i = 0; i < num_vertices; ++i) {
    const auto &g = gaussians[i];
    std::array<float, 6> valpos;
    valpos[0] = g.position[0];
    valpos[1] = g.position[1];
    valpos[2] = g.position[2];    
    valpos[3] = g.scale[0];
    valpos[4] = g.scale[1];
    valpos[5] = g.scale[2];
    out_point_cloud->attribute(att_id_position)
        ->SetAttributeValue(AttributeValueIndex(i), &valpos[0]);

    std::array<uint8_t, 8> valcol;
    for (int j = 0; j < 4; j++) {
      valcol[j] = g.color[j];
    }
    for (int j = 4; j < 8; j++) {
      valcol[j] = g.rotation[j-4];
    }
    out_point_cloud->attribute(att_id_color)
        ->SetAttributeValue(AttributeValueIndex(i), &valcol[0]);
  }
  return OkStatus();
}
}  // namespace draco