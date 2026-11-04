// Copyright 2026 The Draco Authors.
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
#include "draco/io/parser_utils.h"

#include <cstdint>
#include <limits>
#include <string>

#include "draco/core/decoder_buffer.h"
#include "draco/core/draco_test_base.h"

namespace {

bool ParseUnsigned(const std::string& text, uint32_t* value) {
  draco::DecoderBuffer buffer;
  buffer.Init(text.data(), text.size());
  return draco::parser::ParseUnsignedInt(&buffer, value);
}

bool ParseSigned(const std::string& text, int32_t* value) {
  draco::DecoderBuffer buffer;
  buffer.Init(text.data(), text.size());
  return draco::parser::ParseSignedInt(&buffer, value);
}

TEST(ParserUtilsTest, ParseUnsignedIntLimits) {
  uint32_t value = 0;
  ASSERT_TRUE(ParseUnsigned("0", &value));
  ASSERT_EQ(value, 0u);
  ASSERT_TRUE(ParseUnsigned("4294967295", &value));
  ASSERT_EQ(value, std::numeric_limits<uint32_t>::max());
  ASSERT_TRUE(ParseUnsigned("004294967295", &value));
  ASSERT_EQ(value, std::numeric_limits<uint32_t>::max());

  // Values that do not fit in uint32_t must not wrap around.
  ASSERT_FALSE(ParseUnsigned("4294967296", &value));
  ASSERT_FALSE(ParseUnsigned("4294967297", &value));
  ASSERT_FALSE(ParseUnsigned("4294967300", &value));
  ASSERT_FALSE(ParseUnsigned("99999999999999999999", &value));
}

TEST(ParserUtilsTest, ParseSignedIntLimits) {
  int32_t value = 0;
  ASSERT_TRUE(ParseSigned("2147483647", &value));
  ASSERT_EQ(value, std::numeric_limits<int32_t>::max());
  ASSERT_TRUE(ParseSigned("+2147483647", &value));
  ASSERT_EQ(value, std::numeric_limits<int32_t>::max());
  ASSERT_TRUE(ParseSigned("-2147483648", &value));
  ASSERT_EQ(value, std::numeric_limits<int32_t>::min());
  ASSERT_TRUE(ParseSigned("-1", &value));
  ASSERT_EQ(value, -1);

  // Values that do not fit in int32_t must not wrap around.
  ASSERT_FALSE(ParseSigned("2147483648", &value));
  ASSERT_FALSE(ParseSigned("-2147483649", &value));
  ASSERT_FALSE(ParseSigned("4294967297", &value));
  ASSERT_FALSE(ParseSigned("-4294967295", &value));
}

}  // namespace
