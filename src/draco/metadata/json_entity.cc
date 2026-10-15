// Copyright 2024 The Draco Authors.
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
#include "draco/metadata/json_entity.h"

#include <string>
#include <vector>

#ifdef DRACO_TRANSCODER_SUPPORTED

namespace draco {

JsonEntity::JsonEntity() : JsonEntity("") {}

JsonEntity::JsonEntity(const std::string& name)
    : name_(name), type_(OBJECT), integer_(0), double_(0.0), boolean_(false) {}

JsonEntity::JsonEntity(const std::string& name, const std::string& value)
    : JsonEntity(name) {
  SetString(value);
}

JsonEntity::JsonEntity(const std::string& name, const char* value)
    : JsonEntity(name) {
  SetString(value);
}

JsonEntity::JsonEntity(const std::string& name, int value) : JsonEntity(name) {
  SetInteger(value);
}

JsonEntity::JsonEntity(const std::string& name, double value)
    : JsonEntity(name) {
  SetDouble(value);
}

JsonEntity::JsonEntity(const std::string& name, bool value) : JsonEntity(name) {
  SetBoolean(value);
}

bool JsonEntity::operator==(const JsonEntity& other) const {
  if (name_ != other.name_) {
    return false;
  }

  if (type_ != other.type_) {
    // Allow comparison between integer and double numbers.
    if ((type_ == INTEGER && other.type_ == DOUBLE) ||
        (type_ == DOUBLE && other.type_ == INTEGER)) {
      const double number =
          (type_ == INTEGER) ? static_cast<double>(integer_) : double_;
      const double other_number = (other.type_ == INTEGER)
                                      ? static_cast<double>(other.integer_)
                                      : other.double_;
      return number == other_number;
    }
    // Other type mismatches are not equal.
    return false;
  }

  // Types are equal, so proceed with type-specific comparison.
  switch (type_) {
    case OBJECT:
      if (object_.size() != other.object_.size()) {
        return false;
      }
      for (int i = 0; i < object_.size(); ++i) {
        if (object_[i] != other.object_[i]) {
          return false;
        }
      }
      break;
    case ARRAY:
      if (array_.size() != other.array_.size()) {
        return false;
      }
      for (int i = 0; i < array_.size(); ++i) {
        if (array_[i] != other.array_[i]) {
          return false;
        }
      }
      break;
    case STRING:
      return string_ == other.string_;
    case INTEGER:
      return integer_ == other.integer_;
    case DOUBLE:
      return double_ == other.double_;
    case BOOLEAN:
      return boolean_ == other.boolean_;
  }
  return true;
}

bool JsonEntity::operator!=(const JsonEntity& other) const {
  return !(*this == other);
}

void JsonEntity::Copy(const JsonEntity& src) {
  name_ = src.name_;
  type_ = src.type_;
  object_.reserve(src.object_.size());
  for (const JsonEntity& obj : src.object_) {
    object_.emplace_back();
    object_.back().Copy(obj);
  }
  array_.reserve(src.array_.size());
  for (const JsonEntity& obj : src.array_) {
    array_.emplace_back();
    array_.back().Copy(obj);
  }
  string_ = src.string_;
  integer_ = src.integer_;
  double_ = src.double_;
  boolean_ = src.boolean_;
}

const JsonEntity* JsonEntity::GetObjectEntityByName(
    const std::string& name) const {
  for (const JsonEntity& obj : object_) {
    if (obj.GetName() == name) {
      return &obj;
    }
  }
  return nullptr;
}

JsonEntity* JsonEntity::GetObjectEntityByName(const std::string& name) {
  for (JsonEntity& obj : object_) {
    if (obj.GetName() == name) {
      return &obj;
    }
  }
  return nullptr;
}

std::vector<JsonEntity>& JsonEntity::SetObject() {
  type_ = OBJECT;
  return object_;
}

std::vector<JsonEntity>& JsonEntity::SetArray() {
  type_ = ARRAY;
  return array_;
}

void JsonEntity::SetString(const std::string& value) {
  type_ = STRING;
  string_ = value;
}

void JsonEntity::SetInteger(int value) {
  type_ = INTEGER;
  integer_ = value;
}

void JsonEntity::SetDouble(double value) {
  type_ = DOUBLE;
  double_ = value;
}

void JsonEntity::SetBoolean(bool value) {
  type_ = BOOLEAN;
  boolean_ = value;
}

}  // namespace draco

#endif  // DRACO_TRANSCODER_SUPPORTED
