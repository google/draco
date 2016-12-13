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
#ifndef DRACO_COMPRESSION_POINT_CLOUD_ALGORITHMS_FLOAT_POINTS_KD_TREE_DECODER_H_
#define DRACO_COMPRESSION_POINT_CLOUD_ALGORITHMS_FLOAT_POINTS_KD_TREE_DECODER_H_

#include <memory>

#include "compression/point_cloud/algorithms/point_cloud_types.h"
#include "compression/point_cloud/algorithms/quantize_points_3.h"
#include "core/decoder_buffer.h"

namespace draco {

// Decodes a point cloud encoded by PointCloudKdTreeEncoder.
class FloatPointsKdTreeDecoder {
 public:
  FloatPointsKdTreeDecoder();

  // Decodes a point cloud from |buffer|.
  template <class OutputIteratorT>
  bool DecodePointCloud(DecoderBuffer *buffer, OutputIteratorT out);
  // Initializes a DecoderBuffer from |data|, and calls function above.
  template <class OutputIteratorT>
  bool DecodePointCloud(const char *data, size_t data_size,
                        OutputIteratorT out) {
    if (data == 0 || data_size <= 0)
      return false;

    DecoderBuffer buffer;
    buffer.Init(data, data_size);
    return DecodePointCloud(&buffer, out);
  }

  uint32_t quantization_bits() const { return qinfo_.quantization_bits; }
  uint32_t compression_level() const { return compression_level_; }
  float range() const { return qinfo_.range; }
  uint32_t num_points() const { return num_points_; }
  uint32_t version() const { return version_; }
  std::string identification_string() const {
    return "FloatPointsKdTreeDecoder";
  }

 private:
  bool DecodePointCloudInternal(DecoderBuffer *buffer,
                                std::vector<Point3ui> *qpoints);

  static const uint32_t version_ = 2;
  QuantizationInfo qinfo_;
  uint32_t num_points_;
  uint32_t compression_level_;
};

template <class OutputIteratorT>
bool FloatPointsKdTreeDecoder::DecodePointCloud(DecoderBuffer *buffer,
                                                OutputIteratorT out)

{
  std::vector<Point3ui> qpoints;
  if (!DecodePointCloudInternal(buffer, &qpoints))
    return false;

  DequantizePoints3(qpoints.begin(), qpoints.end(), qinfo_, out);

  return true;
}

}  // namespace draco

#undef BIT_DECODER_TYPE

#endif  // DRACO_COMPRESSION_POINT_CLOUD_ALGORITHMS_FLOAT_POINTS_KD_TREE_DECODER_H_
