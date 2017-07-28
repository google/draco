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
#ifndef DRACO_CORE_OPTIONS_H_
#define DRACO_CORE_OPTIONS_H_

#include <map>
#include <string>

namespace draco {

// Class for storing generic options as a <name, value> pair in a string map.
// The API provides helper methods for directly storing values of various types
// such as ints and bools. One named option should be set with only a single
// data type.
class Options {
 public:
  Options();
  void SetInt(const std::string &name, int val);
  void SetBool(const std::string &name, bool val);
  void SetString(const std::string &name, const std::string &val);

  // Getters will return a default value if the entry is not found. The default
  // value can be specified in the overloaded version of each function.
  int GetInt(const std::string &name) const;
  int GetInt(const std::string &name, int default_val) const;
  bool GetBool(const std::string &name) const;
  bool GetBool(const std::string &name, bool default_val) const;
  std::string GetString(const std::string &name) const;
  std::string GetString(const std::string &name,
                        const std::string &default_val) const;

  bool IsOptionSet(const std::string &name) const {
    return options_.count(name) > 0;
  }

 private:
  std::map<std::string, std::string> options_;
};

}  // namespace draco

#endif  // DRACO_CORE_OPTIONS_H_
