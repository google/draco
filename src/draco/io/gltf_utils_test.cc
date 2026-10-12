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
#include <limits>
#include <ostream>
#include <sstream>
#include <string>

#include "draco/core/draco_test_base.h"
#include "draco/core/draco_test_utils.h"
#include "draco/metadata/json_entity.h"

namespace draco {

class GltfUtilsTest : public ::testing::Test {
 protected:
  void CompareGolden(JsonWriter* json_writer, const std::string& golden_str) {
    const std::string json = json_writer->MoveData();
    ASSERT_EQ(golden_str, json);
  }
};

TEST_F(GltfUtilsTest, TestNoData) {
  const std::string golden = "";
  JsonWriter json_writer;
  CompareGolden(&json_writer, golden);
}

TEST_F(GltfUtilsTest, TestValues) {
  JsonWriter json_writer;
  json_writer.OutputValue(0);
  CompareGolden(&json_writer, "0");

  json_writer.Reset();
  json_writer.OutputValue(1);
  CompareGolden(&json_writer, "1");

  json_writer.Reset();
  json_writer.OutputValue(-1);
  CompareGolden(&json_writer, "-1");

  json_writer.Reset();
  json_writer.OutputValue(0.0);
  CompareGolden(&json_writer, "0");

  json_writer.Reset();
  json_writer.OutputValue(1.0);
  CompareGolden(&json_writer, "1");

  json_writer.Reset();
  json_writer.OutputValue(0.25);
  CompareGolden(&json_writer, "0.25");

  json_writer.Reset();
  json_writer.OutputValue(-0.25);
  CompareGolden(&json_writer, "-0.25");

  json_writer.Reset();
  json_writer.OutputValue(false);
  CompareGolden(&json_writer, "false");

  json_writer.Reset();
  json_writer.OutputValue(true);
  CompareGolden(&json_writer, "true");

  json_writer.Reset();
  json_writer.OutputValue("test int", -1);
  CompareGolden(&json_writer, "\"test int\": -1");

  json_writer.Reset();
  json_writer.OutputValue("test float", -10.25);
  CompareGolden(&json_writer, "\"test float\": -10.25");

  json_writer.Reset();
  json_writer.OutputValue("test char*", "I am the string!");
  CompareGolden(&json_writer, "\"test char*\": \"I am the string!\"");

  json_writer.Reset();
  const std::string value = "I am the string!";
  json_writer.OutputValue("test string", value);
  CompareGolden(&json_writer, "\"test string\": \"I am the string!\"");

  json_writer.Reset();
  json_writer.OutputValue("test bool", false);
  CompareGolden(&json_writer, "\"test bool\": false");

  json_writer.Reset();
  json_writer.OutputValue("test bool", true);
  CompareGolden(&json_writer, "\"test bool\": true");
}

TEST_F(GltfUtilsTest, TestSpecialCharacters) {
  JsonWriter json_writer;
  const std::string test_double_quote = "I am double quote\"";
  json_writer.OutputValue("test double quote", test_double_quote);
  CompareGolden(&json_writer,
                "\"test double quote\": \"I am double quote\\\"\"");

  json_writer.Reset();
  const std::string test_backspace = "I am backspace\b";
  json_writer.OutputValue("test backspace", test_backspace);
  CompareGolden(&json_writer, "\"test backspace\": \"I am backspace\\\b\"");

  json_writer.Reset();
  const std::string test_form_feed = "I am form feed\f";
  json_writer.OutputValue("test form feed", test_form_feed);
  CompareGolden(&json_writer, "\"test form feed\": \"I am form feed\\\f\"");

  json_writer.Reset();
  const std::string test_newline = "I am newline\n";
  json_writer.OutputValue("test newline", test_newline);
  CompareGolden(&json_writer, "\"test newline\": \"I am newline\\\n\"");

  json_writer.Reset();
  const std::string test_tab = "I am tab\t";
  json_writer.OutputValue("test tab", test_tab);
  CompareGolden(&json_writer, "\"test tab\": \"I am tab\\\t\"");

  json_writer.Reset();
  const std::string test_backslash = "I am backslash\\";
  json_writer.OutputValue("test backslash", test_backslash);
  CompareGolden(&json_writer, "\"test backslash\": \"I am backslash\\\\\"");

  json_writer.Reset();
  const std::string test_multiple_special_characters = "\"break\"and\\more\"\\";
  json_writer.OutputValue("test multiple_special_characters",
                          test_multiple_special_characters);
  CompareGolden(&json_writer,
                "\"test multiple_special_characters\": "
                "\"\\\"break\\\"and\\\\more\\\"\\\\\"");
}

TEST_F(GltfUtilsTest, TestObjects) {
  JsonWriter json_writer;
  json_writer.BeginObject();
  json_writer.EndObject();
  CompareGolden(&json_writer, "{\n}");

  json_writer.Reset();
  json_writer.BeginObject("object");
  json_writer.EndObject();
  CompareGolden(&json_writer, "\"object\": {\n}");

  json_writer.Reset();
  json_writer.BeginObject("object");
  json_writer.OutputValue(0);
  json_writer.EndObject();
  CompareGolden(&json_writer, "\"object\": {\n  0\n}");

  json_writer.Reset();
  json_writer.BeginObject("object");
  json_writer.OutputValue(0);
  json_writer.OutputValue(1);
  json_writer.OutputValue(2);
  json_writer.OutputValue(3);
  json_writer.EndObject();
  CompareGolden(&json_writer, "\"object\": {\n  0,\n  1,\n  2,\n  3\n}");

  json_writer.Reset();
  json_writer.BeginObject("object1");
  json_writer.EndObject();
  json_writer.BeginObject("object2");
  json_writer.EndObject();
  CompareGolden(&json_writer, "\"object1\": {\n},\n\"object2\": {\n}");

  json_writer.Reset();
  json_writer.BeginObject("object1");
  json_writer.BeginObject("object2");
  json_writer.EndObject();
  json_writer.EndObject();
  CompareGolden(&json_writer, "\"object1\": {\n  \"object2\": {\n  }\n}");
}

TEST_F(GltfUtilsTest, TestArrays) {
  JsonWriter json_writer;
  json_writer.BeginArray("array");
  json_writer.EndArray();
  CompareGolden(&json_writer, "\"array\": [\n]");

  json_writer.Reset();
  json_writer.BeginArray("array");
  json_writer.OutputValue(0);
  json_writer.EndArray();
  CompareGolden(&json_writer, "\"array\": [\n  0\n]");

  json_writer.Reset();
  json_writer.BeginArray("array");
  json_writer.OutputValue(0);
  json_writer.OutputValue(1);
  json_writer.OutputValue(2);
  json_writer.OutputValue(3);
  json_writer.EndArray();
  CompareGolden(&json_writer, "\"array\": [\n  0,\n  1,\n  2,\n  3\n]");

  json_writer.Reset();
  json_writer.BeginArray("array1");
  json_writer.EndArray();
  json_writer.BeginArray("array2");
  json_writer.EndArray();
  CompareGolden(&json_writer, "\"array1\": [\n],\n\"array2\": [\n]");

  json_writer.Reset();
  json_writer.BeginArray("array1");
  json_writer.BeginArray("array2");
  json_writer.EndArray();
  json_writer.EndArray();
  CompareGolden(&json_writer, "\"array1\": [\n  \"array2\": [\n  ]\n]");

  json_writer.Reset();
  json_writer.BeginArray("array1");
  json_writer.BeginArray();
  json_writer.EndArray();
  json_writer.EndArray();
  CompareGolden(&json_writer, "\"array1\": [\n  [\n  ]\n]");
}

TEST_F(GltfUtilsTest, TestGltfValues) {
  JsonWriter json_writer;
  const int8_t int8_value_min = std::numeric_limits<int8_t>::min();
  const int8_t int8_value_max = std::numeric_limits<int8_t>::max();
  const GltfValue int8_value_low(int8_value_min);
  const GltfValue int8_value_high(int8_value_max);
  json_writer.OutputValue(int8_value_low);
  json_writer.OutputValue(int8_value_high);
  CompareGolden(&json_writer, "-128,\n127");

  json_writer.Reset();
  const uint8_t uint8_value_min = std::numeric_limits<uint8_t>::min();
  const uint8_t uint8_value_max = std::numeric_limits<uint8_t>::max();
  const GltfValue uint8_value_low(uint8_value_min);
  const GltfValue uint8_value_high(uint8_value_max);
  json_writer.OutputValue(uint8_value_low);
  json_writer.OutputValue(uint8_value_high);
  CompareGolden(&json_writer, "0,\n255");

  json_writer.Reset();
  const int16_t int16_value_min = std::numeric_limits<int16_t>::min();
  const int16_t int16_value_max = std::numeric_limits<int16_t>::max();
  const GltfValue int16_value_low(int16_value_min);
  const GltfValue int16_value_high(int16_value_max);
  json_writer.OutputValue(int16_value_low);
  json_writer.OutputValue(int16_value_high);
  CompareGolden(&json_writer, "-32768,\n32767");

  json_writer.Reset();
  const uint16_t uint16_value_min = std::numeric_limits<uint16_t>::min();
  const uint16_t uint16_value_max = std::numeric_limits<uint16_t>::max();
  const GltfValue uint16_value_low(uint16_value_min);
  const GltfValue uint16_value_high(uint16_value_max);
  json_writer.OutputValue(uint16_value_low);
  json_writer.OutputValue(uint16_value_high);
  CompareGolden(&json_writer, "0,\n65535");

  json_writer.Reset();
  const uint32_t uint32_value_min = std::numeric_limits<uint32_t>::min();
  const uint32_t uint32_value_max = std::numeric_limits<uint32_t>::max();
  const GltfValue uint32_value_low(uint32_value_min);
  const GltfValue uint32_value_high(uint32_value_max);
  json_writer.OutputValue(uint32_value_low);
  json_writer.OutputValue(uint32_value_high);
  CompareGolden(&json_writer, "0,\n4294967295");

  json_writer.Reset();
  const float float_value_min = std::numeric_limits<float>::min();
  const float float_value_max = std::numeric_limits<float>::max();
  const GltfValue float_value_low(float_value_min);
  const GltfValue float_value_high(float_value_max);
  json_writer.OutputValue(float_value_low);
  json_writer.OutputValue(float_value_high);
  CompareGolden(&json_writer,
                "1.1754943508222875e-38,\n3.4028234663852886e+38");

  json_writer.Reset();
  const GltfValue float_value_0(0.1f);
  const GltfValue float_value_1(1.f);
  json_writer.OutputValue(float_value_0);
  json_writer.OutputValue(float_value_1);
  CompareGolden(&json_writer, "0.10000000149011612,\n1");
}

TEST_F(GltfUtilsTest, TestObjectsCompact) {
  JsonWriter json_writer;
  json_writer.SetMode(JsonWriter::COMPACT);
  json_writer.BeginObject();
  json_writer.EndObject();
  CompareGolden(&json_writer, "{}");

  json_writer.Reset();
  json_writer.BeginObject("object");
  json_writer.EndObject();
  CompareGolden(&json_writer, "\"object\":{}");

  json_writer.Reset();
  json_writer.BeginObject("object");
  json_writer.OutputValue(0);
  json_writer.EndObject();
  CompareGolden(&json_writer, "\"object\":{0}");

  json_writer.Reset();
  json_writer.BeginObject("object");
  json_writer.OutputValue(0);
  json_writer.OutputValue(1);
  json_writer.OutputValue(2);
  json_writer.OutputValue(3);
  json_writer.EndObject();
  CompareGolden(&json_writer, "\"object\":{0,1,2,3}");

  json_writer.Reset();
  json_writer.BeginObject("object1");
  json_writer.EndObject();
  json_writer.BeginObject("object2");
  json_writer.EndObject();
  CompareGolden(&json_writer, "\"object1\":{},\"object2\":{}");

  json_writer.Reset();
  json_writer.BeginObject("object1");
  json_writer.BeginObject("object2");
  json_writer.EndObject();
  json_writer.EndObject();
  CompareGolden(&json_writer, "\"object1\":{\"object2\":{}}");
}

TEST_F(GltfUtilsTest, TestArraysCompact) {
  JsonWriter json_writer;
  json_writer.SetMode(JsonWriter::COMPACT);
  json_writer.BeginArray("array");
  json_writer.EndArray();
  CompareGolden(&json_writer, "\"array\":[]");

  json_writer.Reset();
  json_writer.BeginArray("array");
  json_writer.OutputValue(0);
  json_writer.EndArray();
  CompareGolden(&json_writer, "\"array\":[0]");

  json_writer.Reset();
  json_writer.BeginArray("array");
  json_writer.OutputValue(0);
  json_writer.OutputValue(1);
  json_writer.OutputValue(2);
  json_writer.OutputValue(3);
  json_writer.EndArray();
  CompareGolden(&json_writer, "\"array\":[0,1,2,3]");

  json_writer.Reset();
  json_writer.BeginArray("array1");
  json_writer.EndArray();
  json_writer.BeginArray("array2");
  json_writer.EndArray();
  CompareGolden(&json_writer, "\"array1\":[],\"array2\":[]");

  json_writer.Reset();
  json_writer.BeginArray("array1");
  json_writer.BeginArray("array2");
  json_writer.EndArray();
  json_writer.EndArray();
  CompareGolden(&json_writer, "\"array1\":[\"array2\":[]]");

  json_writer.Reset();
  json_writer.BeginArray("array1");
  json_writer.BeginArray();
  json_writer.EndArray();
  json_writer.EndArray();
  CompareGolden(&json_writer, "\"array1\":[[]]");
}

TEST_F(GltfUtilsTest, TestJsonEntity) {
  JsonWriter json_writer;

  // Test boolean.
  {
    draco::JsonEntity entity("boolean");
    entity.SetBoolean(true);
    json_writer.OutputValue(entity);
    CompareGolden(&json_writer, "\"boolean\": true");
    json_writer.Reset();
  }

  // Test integer.
  {
    draco::JsonEntity entity("integer");
    entity.SetInteger(42);
    json_writer.OutputValue(entity);
    CompareGolden(&json_writer, "\"integer\": 42");
    json_writer.Reset();
  }

  // Test double.
  {
    draco::JsonEntity entity("double");
    entity.SetDouble(-2.718);
    json_writer.OutputValue(entity);
    CompareGolden(&json_writer, "\"double\": -2.718");
    json_writer.Reset();
  }

  // Test string.
  {
    draco::JsonEntity entity("string");
    entity.SetString("Hello world!");
    json_writer.OutputValue(entity);
    CompareGolden(&json_writer, "\"string\": \"Hello world!\"");
    json_writer.Reset();
  }

  // Test arrays.
  json_writer.SetMode(JsonWriter::COMPACT);
  {
    // Test empty array.
    {
      draco::JsonEntity entity("empty");
      entity.SetArray();
      json_writer.OutputValue(entity);
      CompareGolden(&json_writer, "\"empty\":[]");
      json_writer.Reset();
    }

    // Test array of integers.
    {
      draco::JsonEntity entity("array");
      entity.SetArray().push_back(draco::JsonEntity("", 2));
      entity.SetArray().push_back(draco::JsonEntity("", 3));
      entity.SetArray().push_back(draco::JsonEntity("", 5));
      json_writer.OutputValue(entity);
      CompareGolden(&json_writer, "\"array\":[2,3,5]");
      json_writer.Reset();
    }
  }

  // Test objects.
  {
    // Test empty entity.
    {
      draco::JsonEntity entity("empty");
      entity.SetObject();
      json_writer.OutputValue(entity);
      CompareGolden(&json_writer, "\"empty\":{}");
      json_writer.Reset();
    }

    // Test entity with integer fields.
    {
      draco::JsonEntity entity("object");
      entity.SetObject().push_back(draco::JsonEntity("one", 1));
      entity.SetObject().push_back(draco::JsonEntity("two", 2));
      entity.SetObject().push_back(draco::JsonEntity("three", 3));
      json_writer.OutputValue(entity);
      CompareGolden(&json_writer,
                    "\"object\":{\"one\":1,\"two\":2,\"three\":3}");
      json_writer.Reset();
    }
  }

  // Test entities without names.
  json_writer.SetMode(JsonWriter::COMPACT);
  {
    draco::JsonEntity entity;
    entity.SetObject().push_back(draco::JsonEntity("", 42));
    entity.SetObject().push_back(draco::JsonEntity("", false));
    entity.SetObject().push_back(draco::JsonEntity("", -2.718));
    entity.SetObject().push_back(draco::JsonEntity("", "Hello world!"));
    draco::JsonEntity array;
    array.SetArray().push_back(draco::JsonEntity("", 5));
    entity.SetObject().push_back(array);
    draco::JsonEntity object;
    object.SetObject().push_back(draco::JsonEntity("", 6));
    entity.SetObject().push_back(object);
    json_writer.OutputValue(entity);
    CompareGolden(&json_writer, "{42,false,-2.718,\"Hello world!\",[5],{6}}");
    json_writer.Reset();
  }

  // Test nesting.
  json_writer.SetMode(JsonWriter::READABLE);
  {
    draco::JsonEntity entity("object");
    entity.SetObject().emplace_back("array");
    {
      auto& items = entity.SetObject().back().SetArray();
      items.emplace_back("", true);
      items.emplace_back("", 42);
      items.emplace_back("", -2.718);
      items.emplace_back("", "Hello world!");
      items.emplace_back("");
      {
        auto& nested = items.back().SetArray();
        nested.emplace_back("", "a");
        nested.emplace_back("", "b");
        nested.emplace_back("", "c");
      }
      items.emplace_back("");
      {
        auto& fields = items.back().SetObject();
        fields.emplace_back("boolean", true);
        fields.emplace_back("integer", 42);
        fields.emplace_back("double", -2.718);
        fields.emplace_back("string", "Hello world!");
        fields.emplace_back("next");
        {
          auto& items = fields.back().SetArray();
          items.emplace_back("", 1);
          items.emplace_back("", 2);
          items.emplace_back("", 3);
        }
        fields.emplace_back("nested");
        {
          auto& nested = fields.back().SetObject();
          nested.emplace_back("a", true);
          nested.emplace_back("b", false);
        }
      }
    }
    // clang-format off
    std::stringstream ss;
    ss << "\"object\": {"                       << std::endl;
    ss << "  \"array\": ["                      << std::endl;
    ss << "    true,"                           << std::endl;
    ss << "    42,"                             << std::endl;
    ss << "    -2.718,"                         << std::endl;
    ss << "    \"Hello world!\","               << std::endl;
    ss << "    ["                               << std::endl;
    ss << "      \"a\","                        << std::endl;
    ss << "      \"b\","                        << std::endl;
    ss << "      \"c\""                         << std::endl;
    ss << "    ],"                              << std::endl;
    ss << "    {"                               << std::endl;
    ss << "      \"boolean\": true,"            << std::endl;
    ss << "      \"integer\": 42,"              << std::endl;
    ss << "      \"double\": -2.718,"           << std::endl;
    ss << "      \"string\": \"Hello world!\"," << std::endl;
    ss << "      \"next\": ["                   << std::endl;
    ss << "        1,"                          << std::endl;
    ss << "        2,"                          << std::endl;
    ss << "        3"                           << std::endl;
    ss << "      ],"                            << std::endl;
    ss << "      \"nested\": {"                 << std::endl;
    ss << "        \"a\": true,"                << std::endl;
    ss << "        \"b\": false"                << std::endl;
    ss << "      }"                             << std::endl;
    ss << "    }"                               << std::endl;
    ss << "  ]"                                 << std::endl;
    ss << "}";
    // clang-format on
    json_writer.OutputValue(entity);
    CompareGolden(&json_writer, ss.str());
    json_writer.Reset();
  }
}

}  // namespace draco

#endif  // DRACO_TRANSCODER_SUPPORTED
