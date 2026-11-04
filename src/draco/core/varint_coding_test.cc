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
#include <cstdint>
#include <limits>
#include <vector>

#include "draco/core/decoder_buffer.h"
#include "draco/core/draco_test_base.h"
#include "draco/core/encoder_buffer.h"
#include "draco/core/varint_decoding.h"
#include "draco/core/varint_encoding.h"

namespace {

template <typename T>
bool DecodeBytes(const std::vector<uint8_t>& bytes, T* out_val) {
  draco::DecoderBuffer buffer;
  buffer.Init(reinterpret_cast<const char*>(bytes.data()), bytes.size());
  return draco::DecodeVarint(out_val, &buffer);
}

template <typename T>
void TestRoundTrip(T value) {
  draco::EncoderBuffer encoder_buffer;
  ASSERT_TRUE(draco::EncodeVarint(value, &encoder_buffer));
  draco::DecoderBuffer decoder_buffer;
  decoder_buffer.Init(encoder_buffer.data(), encoder_buffer.size());
  T decoded = 0;
  ASSERT_TRUE(draco::DecodeVarint(&decoded, &decoder_buffer));
  ASSERT_EQ(decoded, value);
}

TEST(VarintCodingTest, RoundTrip) {
  TestRoundTrip<uint32_t>(0);
  TestRoundTrip<uint32_t>(127);
  TestRoundTrip<uint32_t>(128);
  TestRoundTrip<uint32_t>(std::numeric_limits<uint32_t>::max());
  TestRoundTrip<uint64_t>(0);
  TestRoundTrip<uint64_t>(uint64_t{1} << 63);
  TestRoundTrip<uint64_t>(std::numeric_limits<uint64_t>::max());
  TestRoundTrip<int32_t>(std::numeric_limits<int32_t>::min());
  TestRoundTrip<int32_t>(std::numeric_limits<int32_t>::max());
  TestRoundTrip<int64_t>(std::numeric_limits<int64_t>::min());
  TestRoundTrip<int64_t>(std::numeric_limits<int64_t>::max());
}

TEST(VarintCodingTest, DecodesLargestValues) {
  uint32_t value32 = 0;
  ASSERT_TRUE(DecodeBytes<uint32_t>({0xff, 0xff, 0xff, 0xff, 0x0f}, &value32));
  ASSERT_EQ(value32, std::numeric_limits<uint32_t>::max());

  uint64_t value64 = 0;
  ASSERT_TRUE(DecodeBytes<uint64_t>(
      {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01}, &value64));
  ASSERT_EQ(value64, std::numeric_limits<uint64_t>::max());
}

TEST(VarintCodingTest, RejectsValuesThatDoNotFit) {
  // 2^32 does not fit in a varUI32. It used to be decoded as 0.
  uint32_t value32 = 0;
  ASSERT_FALSE(DecodeBytes<uint32_t>({0x80, 0x80, 0x80, 0x80, 0x10}, &value32));
  // 2^32 + 5 used to be decoded as 5.
  ASSERT_FALSE(DecodeBytes<uint32_t>({0x85, 0x80, 0x80, 0x80, 0x10}, &value32));
  ASSERT_FALSE(DecodeBytes<uint32_t>({0xff, 0xff, 0xff, 0xff, 0x7f}, &value32));

  // 2^64 does not fit in a varUI64.
  uint64_t value64 = 0;
  ASSERT_FALSE(DecodeBytes<uint64_t>(
      {0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x02}, &value64));

  // The signed variants decode the same unsigned symbol first.
  int32_t signed32 = 0;
  ASSERT_FALSE(DecodeBytes<int32_t>({0x82, 0x80, 0x80, 0x80, 0x10}, &signed32));
}

TEST(VarintCodingTest, AcceptsPaddedEncodings) {
  // A value may still use more bytes than needed, as long as it fits.
  uint32_t value32 = 0;
  ASSERT_TRUE(DecodeBytes<uint32_t>({0x85, 0x80, 0x80, 0x80, 0x00}, &value32));
  ASSERT_EQ(value32, 5u);
}

TEST(VarintCodingTest, RejectsTooManyBytes) {
  uint32_t value32 = 0;
  ASSERT_FALSE(
      DecodeBytes<uint32_t>({0x80, 0x80, 0x80, 0x80, 0x80, 0x00}, &value32));
}

}  // namespace
