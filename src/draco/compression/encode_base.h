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
#ifndef DRACO_SRC_DRACO_COMPRESSION_ENCODE_BASE_H_
#define DRACO_SRC_DRACO_COMPRESSION_ENCODE_BASE_H_

#include "draco/attributes/geometry_attribute.h"
#include "draco/compression/config/compression_shared.h"
#include "draco/core/status.h"

namespace draco {

// Base class for our geometry encoder classes. |EncoderOptionsT| specifies
// options class used by the encoder. Please, see encode.h and expert_encode.h
// for more details and method descriptions.
template <class EncoderOptionsT>
class EncoderBase {
 public:
  typedef EncoderOptionsT OptionsType;

  EncoderBase() : options_(EncoderOptionsT::CreateDefaultOptions()) {}

  const EncoderOptionsT &options() const { return options_; }
  EncoderOptionsT &options() { return options_; }

 protected:
  void Reset(const EncoderOptionsT &options) { options_ = options; }

  void Reset() { options_ = EncoderOptionsT::CreateDefaultOptions(); }

  void SetSpeedOptions(int encoding_speed, int decoding_speed) {
    options_.SetSpeed(encoding_speed, decoding_speed);
  }

  void SetEncodingMethod(int encoding_method) {
    options_.SetGlobalInt("encoding_method", encoding_method);
  }

  Status CheckPredictionScheme(GeometryAttribute::Type att_type,
                               int prediction_scheme) {
    if (prediction_scheme < 0)
      return Status(Status::ERROR, "Invalid prediction scheme requested.");
    if (prediction_scheme >= NUM_PREDICTION_SCHEMES)
      return Status(Status::ERROR, "Invalid prediction scheme requested.");
    if (prediction_scheme == MESH_PREDICTION_TEX_COORDS_DEPRECATED)
      return Status(Status::ERROR,
                    "MESH_PREDICTION_TEX_COORDS_DEPRECATED is deprecated.");
    if (prediction_scheme == MESH_PREDICTION_TEX_COORDS_PORTABLE) {
      if (att_type != GeometryAttribute::TEX_COORD)
        return Status(Status::ERROR,
                      "Invalid prediction scheme for attribute type.");
    }
    if (prediction_scheme == MESH_PREDICTION_GEOMETRIC_NORMAL) {
      if (att_type != GeometryAttribute::NORMAL)
        return Status(Status::ERROR,
                      "Invalid prediction scheme for attribute type.");
    }
    return Status();
  }

 private:
  EncoderOptionsT options_;
};

}  // namespace draco

#endif  // DRACO_SRC_DRACO_COMPRESSION_ENCODE_BASE_H_
