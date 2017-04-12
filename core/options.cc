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
#include "core/options.h"

#include <cstdlib>
#include <string>

namespace {
std::string ValToString(int val) {
  char temp[64];
  sprintf(temp, "%d", val);
  return temp;
}
}  // namespace

namespace draco {

Options::Options() {}

void Options::SetInt(const std::string &name, int val) {
  options_[name] = ValToString(val);
}

void Options::SetBool(const std::string &name, bool val) {
  options_[name] = ValToString(val ? 1 : 0);
}

void Options::SetString(const std::string &name, const std::string &val) {
  options_[name] = val;
}

int Options::GetInt(const std::string &name) const { return GetInt(name, -1); }

int Options::GetInt(const std::string &name, int default_val) const {
  const auto it = options_.find(name);
  if (it == options_.end())
    return default_val;
  return std::atoi(it->second.c_str());
}

bool Options::GetBool(const std::string &name) const {
  return GetBool(name, false);
}

bool Options::GetBool(const std::string &name, bool default_val) const {
  const int ret = GetInt(name, -1);
  if (ret == -1)
    return default_val;
  return static_cast<bool>(ret);
}

std::string Options::GetString(const std::string &name) const {
  return GetString(name, "");
}

std::string Options::GetString(const std::string &name,
                               const std::string &default_val) const {
  const auto it = options_.find(name);
  if (it == options_.end())
    return default_val;
  return it->second;
}

}  // namespace draco
