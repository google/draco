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
#ifndef DRACO_COMPRESSION_CONFIG_ENCODER_OPTIONS_H_
#define DRACO_COMPRESSION_CONFIG_ENCODER_OPTIONS_H_

#include "compression/config/encoding_features.h"
#include "core/options.h"
#include "point_cloud/point_cloud.h"

namespace draco {

// Class encapsuling options used by PointCloudEncoder and its derived classes.
// The encoder can be controller through three different options:
//   1. Global options
//   2. Per attribute options - i.e., options specific to a given attribute.
//   3. Feature options - options determining the available set of features on
//                        the target decoder.
//
// Please refer to mesh/encode.h for helper functions that can be used to
// initialize the options for various use cases.
class EncoderOptions {
 public:
  static EncoderOptions CreateDefaultOptions();

  // Sets the global options that serve to control the overal behavior of an
  // encoder as well as a fallback for attribute options if they are not set.
  void SetGlobalOptions(const Options &o);
  Options *GetGlobalOptions() { return &global_options_; }

  // Sets options for a specific attribute in a target PointCloud.
  void SetAttributeOptions(int32_t att_id, const Options &o);
  Options *GetAttributeOptions(int32_t att_id);

  // Sets options for all attributes of a given type in the target point cloud.
  void SetNamedAttributeOptions(const PointCloud &pc,
                                GeometryAttribute::Type att_type,
                                const Options &o);
  Options *GetNamedAttributeOptions(const PointCloud &pc,
                                    GeometryAttribute::Type att_type);

  // Sets the list of features enabled by the encoder.
  void SetFeatureOptions(const Options &o);

  void SetGlobalInt(const std::string &name, int val);
  void SetGlobalBool(const std::string &name, bool val);
  void SetGlobalString(const std::string &name, const std::string &val);

  int GetGlobalInt(const std::string &name, int default_val) const;
  bool GetGlobalBool(const std::string &name, bool default_val) const;
  std::string GetGlobalString(const std::string &name,
                              const std::string &default_val) const;

  void SetAttributeInt(int32_t att_id, const std::string &name, int val);
  void SetAttributeBool(int32_t att_id, const std::string &name, bool val);
  void SetAttributeString(int32_t att_id, const std::string &name,
                          const std::string &val);

  // Get an option for a specific attribute. If the option is not found in an
  // attribute specific storage, the implementation will return a global option
  // of the given name (if available).
  int GetAttributeInt(int32_t att_id, const std::string &name,
                      int default_val) const;
  bool GetAttributeBool(int32_t att_id, const std::string &name,
                        bool default_val) const;
  std::string GetAttributeString(int32_t att_id, const std::string &name,
                                 const std::string &default_val) const;

  // Returns speed options with default value of 5.
  int GetEncodingSpeed() const { return GetGlobalInt("encoding_speed", 5); }
  int GetDecodingSpeed() const { return GetGlobalInt("decoding_speed", 5); }

  // Returns the maximum speed for both encoding/decoding.
  int GetSpeed() const {
    const int encoding_speed = GetGlobalInt("encoding_speed", -1);
    const int decoding_speed = GetGlobalInt("decoding_speed", -1);
    const int max_speed = std::max(encoding_speed, decoding_speed);
    if (max_speed == -1)
      return 5;  // Default value.
    return max_speed;
  }

  // Sets a given feature as supported or unsupported by the target decoder.
  // Encoder will always use only supported features when encoding the input
  // geometry.
  void SetSupportedFeature(const std::string &name, bool supported) {
    feature_options_.SetBool(name, supported);
  }
  bool IsFeatureSupported(const std::string &name) const;

 private:
  // Use helper methods to construct the encoder options.
  // See CreateDefaultOptions();
  EncoderOptions();

  Options global_options_;

  // Optional options for each of the attribute stored in a point cloud. If an
  // options entry is not found here it will fallback to global_options_.
  std::vector<Options> attribute_options_;

  // List of supported/unsupported features that can be used by the encoder.
  Options feature_options_;
};

}  // namespace draco

#endif  // DRACO_COMPRESSION_CONFIG_ENCODER_OPTIONS_H_
