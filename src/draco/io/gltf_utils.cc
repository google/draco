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
#include "draco/io/gltf_utils.h"

#ifdef DRACO_TRANSCODER_SUPPORTED
namespace draco {

std::ostream &operator<<(std::ostream &os, const GltfValue &value) {
  if (value.type_ == GltfValue::INT) {
    os << value.value_int_;
  } else {
    os << value.value_double_;
  }
  return os;
}

Indent::Indent() : indent_space_count_(2) {}

void Indent::Increase() { indent_ += std::string(indent_space_count_, ' '); }

void Indent::Decrease() { indent_.erase(0, indent_space_count_); }

std::ostream &operator<<(std::ostream &os, const Indent &indent) {
  return os << indent.indent_;
}

void JsonWriter::Reset() {
  last_type_ = START;
  o_.clear();
  o_.str("");
}

void JsonWriter::BeginObject() { BeginObject(""); }

void JsonWriter::BeginObject(const std::string &name) {
  FinishPreviousLine(BEGIN);
  o_ << indent_;
  if (!name.empty()) {
    o_ << "\"" << name << "\": ";
  }
  o_ << "{";
  indent_.Increase();
}

void JsonWriter::EndObject() {
  FinishPreviousLine(END);
  indent_.Decrease();
  o_ << indent_ << "}";
}

void JsonWriter::BeginArray(const std::string &name) {
  FinishPreviousLine(BEGIN);
  o_ << indent_ << "\"" << name << "\": [";
  indent_.Increase();
}

void JsonWriter::EndArray() {
  FinishPreviousLine(END);
  indent_.Decrease();
  o_ << indent_ << "]";
}

void JsonWriter::FinishPreviousLine(OutputType curr_type) {
  if (last_type_ != START) {
    if ((last_type_ == VALUE && curr_type == VALUE) ||
        (last_type_ == VALUE && curr_type == BEGIN) ||
        (last_type_ == END && curr_type == BEGIN) ||
        (last_type_ == END && curr_type == VALUE)) {
      o_ << ",";
    }
    o_ << std::endl;
  }
  last_type_ = curr_type;
}

std::string JsonWriter::MoveData() {
  const std::string str = o_.str();
  o_.str("");
  return str;
}

}  // namespace draco

#endif  // DRACO_TRANSCODER_SUPPORTED
