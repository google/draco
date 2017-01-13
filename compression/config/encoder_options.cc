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
#include "compression/config/encoder_options.h"

namespace draco {

EncoderOptions EncoderOptions::CreateDefaultOptions() {
  EncoderOptions options;
#ifdef DRACO_STANDARD_EDGEBREAKER_SUPPORTED
  options.SetSupportedFeature(features::kEdgebreaker, true);
#endif
#ifdef DRACO_PREDICTIVE_EDGEBREAKER_SUPPORTED
  options.SetSupportedFeature(features::kPredictiveEdgebreaker, true);
#endif
  return options;
}

EncoderOptions::EncoderOptions() {}

void EncoderOptions::SetGlobalOptions(const Options &o) { global_options_ = o; }

void EncoderOptions::SetAttributeOptions(int32_t att_id, const Options &o) {
  if (attribute_options_.size() <= static_cast<size_t>(att_id)) {
    attribute_options_.resize(att_id + 1);
  }
  attribute_options_[att_id] = o;
}

Options *EncoderOptions::GetAttributeOptions(int32_t att_id) {
  if (attribute_options_.size() <= static_cast<size_t>(att_id)) {
    attribute_options_.resize(att_id + 1);
  }
  return &attribute_options_[att_id];
}

void EncoderOptions::SetNamedAttributeOptions(const PointCloud &pc,
                                              GeometryAttribute::Type att_type,
                                              const Options &o) {
  const int att_id = pc.GetNamedAttributeId(att_type);
  if (att_id >= 0)
    SetAttributeOptions(att_id, o);
}

Options *EncoderOptions::GetNamedAttributeOptions(
    const PointCloud &pc, GeometryAttribute::Type att_type) {
  const int att_id = pc.GetNamedAttributeId(att_type);
  if (att_id >= 0)
    return GetAttributeOptions(att_id);
  return nullptr;
}

void EncoderOptions::SetFeatureOptions(const Options &o) {
  feature_options_ = o;
}

void EncoderOptions::SetGlobalInt(const std::string &name, int val) {
  global_options_.SetInt(name, val);
}

void EncoderOptions::SetGlobalBool(const std::string &name, bool val) {
  global_options_.SetBool(name, val);
}

std::string EncoderOptions::GetGlobalString(
    const std::string &name, const std::string &default_val) const {
  return global_options_.GetString(name, default_val);
}

int EncoderOptions::GetGlobalInt(const std::string &name,
                                 int default_val) const {
  return global_options_.GetInt(name, default_val);
}

bool EncoderOptions::GetGlobalBool(const std::string &name,
                                   bool default_val) const {
  return global_options_.GetBool(name, default_val);
}

void EncoderOptions::SetGlobalString(const std::string &name,
                                     const std::string &val) {
  global_options_.SetString(name, val);
}

void EncoderOptions::SetAttributeInt(int32_t att_id, const std::string &name,
                                     int val) {
  if (att_id >= static_cast<int32_t>(attribute_options_.size())) {
    attribute_options_.resize(att_id + 1);
  }
  attribute_options_[att_id].SetInt(name, val);
}

void EncoderOptions::SetAttributeBool(int32_t att_id, const std::string &name,
                                      bool val) {
  if (att_id >= static_cast<int32_t>(attribute_options_.size())) {
    attribute_options_.resize(att_id + 1);
  }
  attribute_options_[att_id].SetBool(name, val);
}

void EncoderOptions::SetAttributeString(int32_t att_id, const std::string &name,
                                        const std::string &val) {
  if (att_id >= static_cast<int32_t>(attribute_options_.size())) {
    attribute_options_.resize(att_id + 1);
  }
  attribute_options_[att_id].SetString(name, val);
}

int EncoderOptions::GetAttributeInt(int32_t att_id, const std::string &name,
                                    int default_val) const {
  if (att_id < static_cast<int32_t>(attribute_options_.size())) {
    if (attribute_options_[att_id].IsOptionSet(name))
      return attribute_options_[att_id].GetInt(name, default_val);
  }
  return GetGlobalInt(name, default_val);
}

bool EncoderOptions::GetAttributeBool(int32_t att_id, const std::string &name,
                                      bool default_val) const {
  if (att_id < static_cast<int32_t>(attribute_options_.size())) {
    if (attribute_options_[att_id].IsOptionSet(name))
      return attribute_options_[att_id].GetBool(name, default_val);
  }
  return GetGlobalBool(name, default_val);
}

std::string EncoderOptions::GetAttributeString(
    int32_t att_id, const std::string &name,
    const std::string &default_val) const {
  if (att_id < static_cast<int32_t>(attribute_options_.size())) {
    if (attribute_options_[att_id].IsOptionSet(name))
      return attribute_options_[att_id].GetString(name, default_val);
  }
  return GetGlobalString(name, default_val);
}

bool EncoderOptions::IsFeatureSupported(const std::string &name) const {
  return feature_options_.GetBool(name);
}

}  // namespace draco
