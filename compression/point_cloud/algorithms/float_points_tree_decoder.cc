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
#include "compression/point_cloud/algorithms/float_points_tree_decoder.h"

#include <algorithm>

#include "compression/point_cloud/algorithms/integer_points_kd_tree_decoder.h"
#include "compression/point_cloud/algorithms/quantize_points_3.h"
#include "core/math_utils.h"
#include "core/quantization_utils.h"

namespace draco {

FloatPointsTreeDecoder::FloatPointsTreeDecoder()
    : num_points_(0), compression_level_(0) {
  qinfo_.quantization_bits = 0;
  qinfo_.range = 0;
}

bool FloatPointsTreeDecoder::DecodePointCloudKdTreeInternal(
    DecoderBuffer *buffer, std::vector<Point3ui> *qpoints) {
  if (!buffer->Decode(&qinfo_.quantization_bits))
    return false;
  if (!buffer->Decode(&qinfo_.range))
    return false;
  if (!buffer->Decode(&num_points_))
    return false;
  if (!buffer->Decode(&compression_level_))
    return false;

  if (num_points_ > 0) {
    switch (compression_level_) {
      case 0: {
        qpoints->reserve(num_points_);
        IntegerPointsKdTreeDecoder<Point3ui, 0> qpoints_decoder;
        qpoints_decoder.DecodePoints(buffer, std::back_inserter(*qpoints));
        break;
      }
      case 1: {
        qpoints->reserve(num_points_);
        IntegerPointsKdTreeDecoder<Point3ui, 1> qpoints_decoder;
        qpoints_decoder.DecodePoints(buffer, std::back_inserter(*qpoints));
        break;
      }
      case 2: {
        qpoints->reserve(num_points_);
        IntegerPointsKdTreeDecoder<Point3ui, 2> qpoints_decoder;
        qpoints_decoder.DecodePoints(buffer, std::back_inserter(*qpoints));
        break;
      }
      case 3: {
        qpoints->reserve(num_points_);
        IntegerPointsKdTreeDecoder<Point3ui, 3> qpoints_decoder;
        qpoints_decoder.DecodePoints(buffer, std::back_inserter(*qpoints));
        break;
      }
      case 4: {
        qpoints->reserve(num_points_);
        IntegerPointsKdTreeDecoder<Point3ui, 4> qpoints_decoder;
        qpoints_decoder.DecodePoints(buffer, std::back_inserter(*qpoints));
        break;
      }
      case 5: {
        qpoints->reserve(num_points_);
        IntegerPointsKdTreeDecoder<Point3ui, 5> qpoints_decoder;
        qpoints_decoder.DecodePoints(buffer, std::back_inserter(*qpoints));
        break;
      }
      case 6: {
        qpoints->reserve(num_points_);
        IntegerPointsKdTreeDecoder<Point3ui, 6> qpoints_decoder;
        qpoints_decoder.DecodePoints(buffer, std::back_inserter(*qpoints));
        break;
      }
      case 7: {
        qpoints->reserve(num_points_);
        IntegerPointsKdTreeDecoder<Point3ui, 7> qpoints_decoder;
        qpoints_decoder.DecodePoints(buffer, std::back_inserter(*qpoints));
        break;
      }
      case 8: {
        qpoints->reserve(num_points_);
        IntegerPointsKdTreeDecoder<Point3ui, 8> qpoints_decoder;
        qpoints_decoder.DecodePoints(buffer, std::back_inserter(*qpoints));
        break;
      }
      case 9: {
        qpoints->reserve(num_points_);
        IntegerPointsKdTreeDecoder<Point3ui, 9> qpoints_decoder;
        qpoints_decoder.DecodePoints(buffer, std::back_inserter(*qpoints));
        break;
      }
      default: {
        qpoints->reserve(num_points_);
        IntegerPointsKdTreeDecoder<Point3ui, 10> qpoints_decoder;
        qpoints_decoder.DecodePoints(buffer, std::back_inserter(*qpoints));
        break;
      }
    }
  }

  if (qpoints->size() != num_points_)
    return false;
  return true;
}


}  // namespace draco
