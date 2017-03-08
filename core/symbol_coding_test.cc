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
#include "core/decoder_buffer.h"
#include "core/draco_test_base.h"
#include "core/encoder_buffer.h"
#include "core/symbol_decoding.h"
#include "core/symbol_encoding.h"

namespace draco {

class SymbolCodingTest : public ::testing::Test {
 protected:
  SymbolCodingTest() {}
};

TEST_F(SymbolCodingTest, TestLargeNumbers) {
  // This test verifies that SymbolCoding successfully encodes an array of large
  // numbers.
  const uint32_t in[] = {12345678, 1223333, 111, 5};
  const int num_values = sizeof(in) / sizeof(uint32_t);
  EncoderBuffer eb;
  ASSERT_TRUE(EncodeSymbols(in, num_values, 1, &eb));

  std::vector<uint32_t> out;
  out.resize(num_values);
  DecoderBuffer db;
  db.Init(eb.data(), eb.size());
  ASSERT_TRUE(DecodeSymbols(num_values, 1, &db, &out[0]));
  for (int i = 0; i < num_values; ++i) {
    EXPECT_EQ(in[i], out[i]);
  }
}

TEST_F(SymbolCodingTest, TestManyNumbers) {
  // This test verifies that SymbolCoding successfully encodes an array of
  // several numbers that repeat many times.

  // Value/frequency pairs.
  const std::pair<uint32_t, uint32_t> in[] = {
      {12, 1500}, {1025, 31000}, {7, 1}, {9, 5}, {0, 6432}};

  const int num_pairs = sizeof(in) / sizeof(std::pair<uint32_t, uint32_t>);

  std::vector<uint32_t> in_values;
  for (int i = 0; i < num_pairs; ++i) {
    in_values.insert(in_values.end(), in[i].second, in[i].first);
  }
  EncoderBuffer eb;
  ASSERT_TRUE(EncodeSymbols(in_values.data(), in_values.size(), 1, &eb));
  std::vector<uint32_t> out_values;
  out_values.resize(in_values.size());
  DecoderBuffer db;
  db.Init(eb.data(), eb.size());
  ASSERT_TRUE(DecodeSymbols(in_values.size(), 1, &db, &out_values[0]));
  for (uint32_t i = 0; i < in_values.size(); ++i) {
    ASSERT_EQ(in_values[i], out_values[i]);
  }
}

TEST_F(SymbolCodingTest, TestEmpty) {
  // This test verifies that SymbolCoding successfully encodes an empty array.
  EncoderBuffer eb;
  ASSERT_TRUE(EncodeSymbols(nullptr, 0, 1, &eb));
  DecoderBuffer db;
  db.Init(eb.data(), eb.size());
  ASSERT_TRUE(DecodeSymbols(0, 1, &db, nullptr));
}

TEST_F(SymbolCodingTest, TestOneSymbol) {
  // This test verifies that SymbolCoding successfully encodes an a single
  // symbol.
  EncoderBuffer eb;
  const std::vector<uint32_t> in(1200, 0);
  ASSERT_TRUE(EncodeSymbols(in.data(), in.size(), 1, &eb));

  std::vector<uint32_t> out(in.size());
  DecoderBuffer db;
  db.Init(eb.data(), eb.size());
  ASSERT_TRUE(DecodeSymbols(in.size(), 1, &db, &out[0]));
  for (uint32_t i = 0; i < in.size(); ++i) {
    ASSERT_EQ(in[i], out[i]);
  }
}

TEST_F(SymbolCodingTest, TestBitLengths) {
  // This test verifies that SymbolCoding successfully encodes symbols of
  // various bitlengths
  EncoderBuffer eb;
  std::vector<uint32_t> in;
  constexpr int bit_lengths = 18;
  for (int i = 0; i < bit_lengths; ++i) {
    in.push_back(1 << i);
  }
  std::vector<uint32_t> out(in.size());
  for (int i = 0; i < bit_lengths; ++i) {
    eb.Clear();
    ASSERT_TRUE(EncodeSymbols(in.data(), i + 1, 1, &eb));
    DecoderBuffer db;
    db.Init(eb.data(), eb.size());
    ASSERT_TRUE(DecodeSymbols(i + 1, 1, &db, &out[0]));
    for (int j = 0; j < i + 1; ++j) {
      ASSERT_EQ(in[j], out[j]);
    }
  }
}

TEST_F(SymbolCodingTest, TestLargeNumberCondition) {
  // This test verifies that SymbolCoding successfully encodes large symbols
  // that are on the boundary between raw scheme and tagged scheme (18 bits).
  EncoderBuffer eb;
  constexpr int num_symbols = 1000000;
  const std::vector<uint32_t> in(num_symbols, 1 << 18);
  ASSERT_TRUE(EncodeSymbols(in.data(), in.size(), 1, &eb));

  std::vector<uint32_t> out(in.size());
  DecoderBuffer db;
  db.Init(eb.data(), eb.size());
  ASSERT_TRUE(DecodeSymbols(in.size(), 1, &db, &out[0]));
  for (uint32_t i = 0; i < in.size(); ++i) {
    ASSERT_EQ(in[i], out[i]);
  }
}

}  // namespace draco
