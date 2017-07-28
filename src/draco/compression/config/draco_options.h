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
#ifndef DRACO_SRC_DRACO_COMPRESSION_CONFIG_DRACO_OPTIONS_H_
#define DRACO_SRC_DRACO_COMPRESSION_CONFIG_DRACO_OPTIONS_H_

#include <map>
#include <memory>

#include "draco/core/options.h"

namespace draco {

// Base option class used to control encoding and decoding. The geometry coding
// can be controlled through the following options:
//   1. Global options - Options specific to overall geometry or options common
//                       for all attributes
//   2. Per attribute options - Options specific to a given attribute.
//                              Each attribute is identified by the template
//                              argument AttributeKeyT that can be for example
//                              the attribute type or the attribute id.
//
// Example:
//
//   DracoOptions<AttributeKey> options;
//
//   // Set an option common for all attributes.
//   options.SetGlobalInt("some_option_name", 2);
//
//   // Geometry with two attributes.
//   AttributeKey att_key0 = in_key0;
//   AttributeKey att_key1 = in_key1;
//
//   options.SetAttributeInt(att_key0, "some_option_name", 3);
//
//   options.GetAttributeInt(att_key0, "some_option_name");  // Returns 3
//   options.GetAttributeInt(att_key1, "some_option_name");  // Returns 2
//   options.GetGlobalInt("some_option_name");               // Returns 2
//
template <typename AttributeKeyT>
class DracoOptions {
 public:
  typedef AttributeKeyT AttributeKey;

  // Get an option for a specific attribute key. If the option is not found in
  // an attribute specific storage, the implementation will return a global
  // option of the given name (if available). If the option is not found, the
  // provided default value |default_val| is returned instead.
  int GetAttributeInt(const AttributeKey &att_key, const std::string &name,
                      int default_val) const;

  // Sets an option for a specific attribute key.
  void SetAttributeInt(const AttributeKey &att_key, const std::string &name,
                       int val);

  bool GetAttributeBool(const AttributeKey &att_key, const std::string &name,
                        bool default_val) const;
  void SetAttributeBool(const AttributeKey &att_key, const std::string &name,
                        bool val);

  // Gets/sets a global option that is not specific to any attribute.
  int GetGlobalInt(const std::string &name, int default_val) const {
    return global_options_.GetInt(name, default_val);
  }
  void SetGlobalInt(const std::string &name, int val) {
    global_options_.SetInt(name, val);
  }
  bool GetGlobalBool(const std::string &name, bool default_val) const {
    return global_options_.GetBool(name, default_val);
  }
  void SetGlobalBool(const std::string &name, bool val) {
    global_options_.SetBool(name, val);
  }

  // Sets or replaces attribute options with the provided |options|.
  void SetAttributeOptions(const AttributeKey &att_key, const Options &options);
  void SetGlobalOptions(const Options &options) { global_options_ = options; }

  // Returns |Options| instance for the specified options class if it exists.
  const Options *FindAttributeOptions(const AttributeKeyT &att_key) const;
  const Options &GetGlobalOptions() const { return global_options_; }

 private:
  Options *GetAttributeOptions(const AttributeKeyT &att_key);

  Options global_options_;

  // Storage for options related to geometry attributes.
  std::map<AttributeKey, Options> attribute_options_;
};

template <typename AttributeKeyT>
const Options *DracoOptions<AttributeKeyT>::FindAttributeOptions(
    const AttributeKeyT &att_key) const {
  auto it = attribute_options_.find(att_key);
  if (it == attribute_options_.end()) {
    return nullptr;
  }
  return &it->second;
}

template <typename AttributeKeyT>
Options *DracoOptions<AttributeKeyT>::GetAttributeOptions(
    const AttributeKeyT &att_key) {
  auto it = attribute_options_.find(att_key);
  if (it != attribute_options_.end()) {
    return &it->second;
  }
  Options new_options;
  it = attribute_options_.insert(std::make_pair(att_key, new_options)).first;
  return &it->second;
}

template <typename AttributeKeyT>
int DracoOptions<AttributeKeyT>::GetAttributeInt(const AttributeKeyT &att_key,
                                                 const std::string &name,
                                                 int default_val) const {
  const Options *const att_options = FindAttributeOptions(att_key);
  if (att_options && att_options->IsOptionSet(name))
    return att_options->GetInt(name, default_val);
  return global_options_.GetInt(name, default_val);
}

template <typename AttributeKeyT>
void DracoOptions<AttributeKeyT>::SetAttributeInt(const AttributeKeyT &att_key,
                                                  const std::string &name,
                                                  int val) {
  GetAttributeOptions(att_key)->SetInt(name, val);
}

template <typename AttributeKeyT>
bool DracoOptions<AttributeKeyT>::GetAttributeBool(const AttributeKeyT &att_key,
                                                   const std::string &name,
                                                   bool default_val) const {
  const Options *const att_options = FindAttributeOptions(att_key);
  if (att_options && att_options->IsOptionSet(name))
    return att_options->GetBool(name, default_val);
  return global_options_.GetBool(name, default_val);
}

template <typename AttributeKeyT>
void DracoOptions<AttributeKeyT>::SetAttributeBool(const AttributeKeyT &att_key,
                                                   const std::string &name,
                                                   bool val) {
  GetAttributeOptions(att_key)->SetBool(name, val);
}

template <typename AttributeKeyT>
void DracoOptions<AttributeKeyT>::SetAttributeOptions(
    const AttributeKey &att_key, const Options &options) {
  Options *att_options = GetAttributeOptions(att_key);
  *att_options = options;
}

}  // namespace draco

#endif  // DRACO_SRC_DRACO_COMPRESSION_CONFIG_DRACO_OPTIONS_H_
