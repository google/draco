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
#ifndef DRACO_METADATA_JSON_ENTITY_H_
#define DRACO_METADATA_JSON_ENTITY_H_

#include "draco/draco_features.h"

#ifdef DRACO_TRANSCODER_SUPPORTED

#include <string>
#include <vector>

namespace draco {

// JSON entity used to store pass-through glTF extension contents and unparsed
// parts of other extensions like the EXT_structural_metadata schema.
// TODO(vytyaz): Replace StructuralMetadataSchema::Object with this class.
// TODO(vytyaz): Consider using a third_party/json library. Currently there is a
// conflict between Filament's assert_invariant() macro and JSON library's
// assert_invariant() method that causes compile errors in Draco visualization
// library.
class JsonEntity {
 public:
  enum Type { OBJECT, ARRAY, STRING, INTEGER, DOUBLE, BOOLEAN };

  // Constructors.
  JsonEntity();
  explicit JsonEntity(const std::string& name);
  JsonEntity(const std::string& name, const std::string& value);
  JsonEntity(const std::string& name, const char* value);
  JsonEntity(const std::string& name, int value);
  JsonEntity(const std::string& name, double value);
  JsonEntity(const std::string& name, bool value);

  // Methods for comparing two entities.
  bool operator==(const JsonEntity& other) const;
  bool operator!=(const JsonEntity& other) const;

  // Method for copying the entity.
  void Copy(const JsonEntity& src);

  // Methods for getting entity name and type.
  const std::string& GetName() const { return name_; }
  Type GetType() const { return type_; }

  // Methods for getting the entity contents.
  const std::vector<JsonEntity>& GetObject() const { return object_; }
  std::vector<JsonEntity>& GetObject() { return object_; }
  const std::vector<JsonEntity>& GetArray() const { return array_; }
  std::vector<JsonEntity>& GetArray() { return array_; }
  const std::string& GetString() const { return string_; }
  std::string& GetString() { return string_; }
  int GetInteger() const { return integer_; }
  double GetDouble() const { return double_; }
  bool GetBoolean() const { return boolean_; }

  // Looks for a child object entity matching the given |name|, non-recursively.
  // If no field is found, returns nullptr.
  const JsonEntity* GetObjectEntityByName(const std::string& name) const;
  JsonEntity* GetObjectEntityByName(const std::string& name);

  // Methods for setting the value contents.
  std::vector<JsonEntity>& SetObject();
  std::vector<JsonEntity>& SetArray();
  void SetString(const std::string& value);
  void SetInteger(int value);
  void SetDouble(double value);
  void SetBoolean(bool value);

 private:
  std::string name_;
  Type type_;
  std::vector<JsonEntity> object_;
  std::vector<JsonEntity> array_;
  std::string string_;
  int integer_;
  double double_;
  bool boolean_;
};

}  // namespace draco

#endif  // DRACO_TRANSCODER_SUPPORTED
#endif  // DRACO_METADATA_JSON_ENTITY_H_
