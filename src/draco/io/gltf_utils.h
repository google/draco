// Copyright 2018 The Draco Authors.
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
#ifndef DRACO_IO_GLTF_UTILS_H_
#define DRACO_IO_GLTF_UTILS_H_

#include "draco/draco_features.h"

#ifdef DRACO_TRANSCODER_SUPPORTED
#include <iomanip>
#include <sstream>
#include <string>

namespace draco {

// Class used to store integer or float values supported by glTF.
class GltfValue {
 public:
  enum ValueType { INT, DOUBLE };

  explicit GltfValue(int8_t value)
      : type_(INT), value_int_(value), value_double_(-1.0) {}

  explicit GltfValue(uint8_t value)
      : type_(INT), value_int_(value), value_double_(-1.0) {}

  explicit GltfValue(int16_t value)
      : type_(INT), value_int_(value), value_double_(-1.0) {}

  explicit GltfValue(uint16_t value)
      : type_(INT), value_int_(value), value_double_(-1.0) {}

  explicit GltfValue(uint32_t value)
      : type_(INT), value_int_(value), value_double_(-1.0) {}

  explicit GltfValue(float value)
      : type_(DOUBLE), value_int_(-1), value_double_(value) {}

  friend std::ostream &operator<<(std::ostream &os, const GltfValue &value);

 private:
  ValueType type_;
  int64_t value_int_;
  double value_double_;
};

// Utility class used to help with indentation of glTF file.
class Indent {
 public:
  Indent();

  void Increase();
  void Decrease();

  friend std::ostream &operator<<(std::ostream &os, const Indent &indent);

 private:
  // Variables used for spacing of the glTF file.
  std::string indent_;
  const int indent_space_count_;
};

// Class used to keep track of the json state.
class JsonWriter {
 public:
  enum OutputType { START, BEGIN, END, VALUE };

  JsonWriter() : last_type_(START) {}

  // Clear the stringstream and set last type to START.
  void Reset();

  // Every call to BeginObject should have a matching call to EndObject.
  void BeginObject();
  void BeginObject(const std::string &name);
  void EndObject();

  // Every call to BeginArray should have a matching call to EndArray.
  void BeginArray(const std::string &name);
  void EndArray();

  template <typename T>
  void OutputValue(const T &value) {
    FinishPreviousLine(VALUE);
    o_ << indent_ << std::setprecision(17) << value;
  }

  void OutputValue(const bool &value) {
    FinishPreviousLine(VALUE);
    o_ << indent_ << ToString(value);
  }

  void OutputValue(const std::string &name) {
    FinishPreviousLine(VALUE);
    o_ << indent_ << "\"" << name << "\"";
  }

  void OutputValue(const std::string &name, const std::string &value) {
    FinishPreviousLine(VALUE);
    o_ << indent_ << "\"" << name << "\": \"" << value << "\"";
  }

  void OutputValue(const std::string &name, const char *value) {
    FinishPreviousLine(VALUE);
    o_ << indent_ << "\"" << name << "\": \"" << value << "\"";
  }

  template <typename T>
  void OutputValue(const std::string &name, const T &value) {
    FinishPreviousLine(VALUE);
    o_ << indent_ << "\"" << name << "\": " << value;
  }

  void OutputValue(const std::string &name, const bool &value) {
    FinishPreviousLine(VALUE);
    o_ << indent_ << "\"" << name << "\": " << ToString(value);
  }

  // Return the current output and then clear the stringstream.
  std::string MoveData();

 private:
  // Check if a comma needs to be added to the output and then add a new line.
  void FinishPreviousLine(OutputType curr_type);

  // Returns string representation of a Boolean |value|.
  static std::string ToString(bool value) { return value ? "true" : "false"; }

  std::stringstream o_;
  Indent indent_;
  OutputType last_type_;
};

}  // namespace draco

#endif  // DRACO_TRANSCODER_SUPPORTED
#endif  // DRACO_IO_GLTF_UTILS_H_
