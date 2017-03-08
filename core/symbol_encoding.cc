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
#include "core/symbol_encoding.h"

#include <algorithm>
#include <cmath>

#include "core/bit_utils.h"
#include "core/rans_symbol_encoder.h"

namespace draco {

constexpr int32_t kMaxTagSymbolBitLength = 32;
constexpr int kMaxRawEncodingBitLength = 18;

typedef uint64_t TaggedBitLengthFrequencies[kMaxTagSymbolBitLength];

void ConvertSignedIntsToSymbols(const int32_t *in, int in_values,
                                uint32_t *out) {
  // Convert the quantized values into a format more suitable for entropy
  // encoding.
  // Put the sign bit into LSB pos and shift the rest one bit left.
  for (int i = 0; i < in_values; ++i) {
    out[i] = ConvertSignedIntToSymbol(in[i]);
  }
}

uint32_t ConvertSignedIntToSymbol(int32_t val) {
  const bool is_negative = (val < 0);
  if (is_negative)
    val = -val - 1;  // Map -1 to 0, -2 to -1, etc..
  val <<= 1;
  if (is_negative)
    val |= 1;
  return static_cast<uint32_t>(val);
}

// Computes bit lengths of the input values. If num_components > 1, the values
// are processed in "num_components" sized chunks and the bit length is always
// computed for the largest value from the chunk.
static void ComputeBitLengths(const uint32_t *symbols, int num_values,
                              int num_components,
                              std::vector<int> *out_bit_lengths,
                              uint32_t *out_max_value) {
  out_bit_lengths->reserve(num_values);
  *out_max_value = 0;
  // Maximum integer value across all components.
  for (int i = 0; i < num_values; i += num_components) {
    // Get the maximum value for a given entry across all attribute components.
    uint32_t max_component_value = symbols[i];
    for (int j = 1; j < num_components; ++j) {
      if (max_component_value < symbols[i + j])
        max_component_value = symbols[i + j];
    }
    int value_msb_pos = 0;
    if (max_component_value > 0) {
      value_msb_pos = bits::MostSignificantBit(max_component_value);
    }
    if (max_component_value > *out_max_value) {
      *out_max_value = max_component_value;
    }
    out_bit_lengths->push_back(value_msb_pos + 1);
  }
}

template <template <int> class SymbolEncoderT>
bool EncodeTaggedSymbols(const uint32_t *symbols, int num_values,
                         int num_components,
                         const std::vector<int> &bit_lengths,
                         EncoderBuffer *target_buffer);

template <template <int> class SymbolEncoderT>
bool EncodeRawSymbols(const uint32_t *symbols, int num_values,
                      const uint32_t *max_value, EncoderBuffer *target_buffer);

bool EncodeSymbols(const uint32_t *symbols, int num_values, int num_components,
                   EncoderBuffer *target_buffer) {
  if (num_values < 0)
    return false;
  if (num_values == 0)
    return true;
  if (num_components <= 0)
    num_components = 1;
  std::vector<int> bit_lengths;
  uint32_t max_value;
  ComputeBitLengths(symbols, num_values, num_components, &bit_lengths,
                    &max_value);

  // Compute the total bit length used by all values. This will be used for
  // computing a heuristic that chooses the optimal entropy encoding scheme.
  uint64_t total_bit_length = 0;
  for (size_t i = 0; i < bit_lengths.size(); ++i) {
    total_bit_length += bit_lengths[i];
  }

  const int64_t num_component_values = num_values / num_components;

  // The average number of bits necessary for encoding a single entry value.
  const int64_t average_bit_length =
      static_cast<int64_t>(ceil(static_cast<double>(total_bit_length) /
                                static_cast<double>(num_component_values)));
  // The estimated average number of bits necessary for encoding a single
  // bit-length tag.
  int64_t average_bits_per_tag = static_cast<int64_t>(
      ceil(static_cast<float>(bits::MostSignificantBit(average_bit_length)) /
           static_cast<float>(num_components)));
  if (average_bits_per_tag <= 0)
    average_bits_per_tag = 1;

  // Estimate the number of bits needed for encoding the values using the tagged
  // scheme. 32 * 8 is the overhead for encoding the entropy encoding table.
  const int64_t tagged_scheme_total_bits =
      num_component_values *
          (num_components * average_bit_length + average_bits_per_tag) +
      32 * 8;

  // Estimate the number of bits needed by the "raw" scheme. In this case,
  // max_value * 8 is the overhead of the entropy table.
  const int64_t raw_scheme_total_bits =
      num_values * average_bit_length + max_value * 8;

  // The maximum bit length of a single entry value that we can encode using
  // the raw scheme.
  const int max_value_bit_length = bits::MostSignificantBit(max_value) + 1;

  if (tagged_scheme_total_bits < raw_scheme_total_bits ||
      max_value_bit_length > kMaxRawEncodingBitLength) {
    // Use the tagged scheme.
    target_buffer->Encode(static_cast<uint8_t>(0));
    return EncodeTaggedSymbols<RAnsSymbolEncoder>(
        symbols, num_values, num_components, bit_lengths, target_buffer);
  }
  // Else use the raw scheme.
  target_buffer->Encode(static_cast<uint8_t>(1));
  return EncodeRawSymbols<RAnsSymbolEncoder>(symbols, num_values, &max_value,
                                             target_buffer);
}

template <template <int> class SymbolEncoderT>
bool EncodeTaggedSymbols(const uint32_t *symbols, int num_values,
                         int num_components,
                         const std::vector<int> &bit_lengths,
                         EncoderBuffer *target_buffer) {
  // Create entries for entropy coding. Each entry corresponds to a different
  // number of bits that are necessary to encode a given value. Every value
  // has at most 32 bits. Therefore, we need 32 different entries (for
  // bit_lengts [1-32]). For each entry we compute the frequency of a given
  // bit-length in our data set.
  TaggedBitLengthFrequencies frequencies;
  // Set frequency for each entry to zero.
  memset(frequencies, 0, sizeof(frequencies));

  // Compute the frequencies from input data.
  // Maximum integer value for the values across all components.
  for (size_t i = 0; i < bit_lengths.size(); ++i) {
    // Update the frequency of the associated entry id.
    ++frequencies[bit_lengths[i]];
  }

  // Create one extra buffer to store raw value.
  EncoderBuffer value_buffer;
  // Number of expected bits we need to store the values (can be optimized if
  // needed).
  const uint64_t value_bits = kMaxTagSymbolBitLength * num_values;

  // Create encoder for encoding the bit tags.
  SymbolEncoderT<5> tag_encoder;
  tag_encoder.Create(frequencies, kMaxTagSymbolBitLength, target_buffer);

  // Start encoding bit tags.
  tag_encoder.StartEncoding(target_buffer);

  // Also start encoding the values.
  value_buffer.StartBitEncoding(value_bits, false);

  if (tag_encoder.needs_reverse_encoding()) {
    // Encoder needs the values to be encoded in the reverse order.
    for (int i = num_values - num_components; i >= 0; i -= num_components) {
      const int bit_length = bit_lengths[i / num_components];
      tag_encoder.EncodeSymbol(bit_length);

      // Values are always encoded in the normal order
      const int j = num_values - num_components - i;
      const int value_bit_length = bit_lengths[j / num_components];
      for (int c = 0; c < num_components; ++c) {
        value_buffer.EncodeLeastSignificantBits32(value_bit_length,
                                                  symbols[j + c]);
      }
    }
  } else {
    for (int i = 0; i < num_values; i += num_components) {
      const int bit_length = bit_lengths[i / num_components];
      // First encode the tag.
      tag_encoder.EncodeSymbol(bit_length);
      // Now encode all values using the stored bit_length.
      for (int j = 0; j < num_components; ++j) {
        value_buffer.EncodeLeastSignificantBits32(bit_length, symbols[i + j]);
      }
    }
  }
  tag_encoder.EndEncoding(target_buffer);
  value_buffer.EndBitEncoding();

  // Append the values to the end of the target buffer.
  target_buffer->Encode(value_buffer.data(), value_buffer.size());
  return true;
}

template <class SymbolEncoderT>
bool EncodeRawSymbolsInternal(const uint32_t *symbols, int num_values,
                              const uint32_t &max_entry_value,
                              EncoderBuffer *target_buffer) {
  // Count the frequency of each entry value.
  std::vector<uint64_t> frequencies(max_entry_value + 1, 0);
  for (int i = 0; i < num_values; ++i) {
    ++frequencies[symbols[i]];
  }

  SymbolEncoderT encoder;
  encoder.Create(frequencies.data(), frequencies.size(), target_buffer);

  encoder.StartEncoding(target_buffer);
  // Encode all values.
  if (SymbolEncoderT::needs_reverse_encoding()) {
    for (int i = num_values - 1; i >= 0; --i) {
      encoder.EncodeSymbol(symbols[i]);
    }
  } else {
    for (int i = 0; i < num_values; ++i) {
      encoder.EncodeSymbol(symbols[i]);
    }
  }
  encoder.EndEncoding(target_buffer);
  return true;
}

template <template <int> class SymbolEncoderT>
bool EncodeRawSymbols(const uint32_t *symbols, int num_values,
                      const uint32_t *max_value, EncoderBuffer *target_buffer) {
  uint32_t max_entry_value = 0;
  // If the max_value is not provided, find it.
  if (max_value != nullptr) {
    max_entry_value = *max_value;
  } else {
    for (int i = 0; i < num_values; ++i) {
      if (symbols[i] > max_entry_value) {
        max_entry_value = symbols[i];
      }
    }
  }
  int max_value_bits = 0;
  if (max_entry_value > 0) {
    max_value_bits = bits::MostSignificantBit(max_entry_value);
  }
  const int max_value_bit_length = max_value_bits + 1;
  // Currently, we don't support encoding of values larger than 2^18.
  if (max_value_bit_length > kMaxRawEncodingBitLength)
    return false;
  target_buffer->Encode(static_cast<uint8_t>(max_value_bit_length));
  // Use appropriate symbol encoder based on the maximum symbol bit length.
  switch (max_value_bit_length) {
    case 0:
      FALLTHROUGH_INTENDED;
    case 1:
      return EncodeRawSymbolsInternal<SymbolEncoderT<1>>(
          symbols, num_values, max_entry_value, target_buffer);
    case 2:
      return EncodeRawSymbolsInternal<SymbolEncoderT<2>>(
          symbols, num_values, max_entry_value, target_buffer);
    case 3:
      return EncodeRawSymbolsInternal<SymbolEncoderT<3>>(
          symbols, num_values, max_entry_value, target_buffer);
    case 4:
      return EncodeRawSymbolsInternal<SymbolEncoderT<4>>(
          symbols, num_values, max_entry_value, target_buffer);
    case 5:
      return EncodeRawSymbolsInternal<SymbolEncoderT<5>>(
          symbols, num_values, max_entry_value, target_buffer);
    case 6:
      return EncodeRawSymbolsInternal<SymbolEncoderT<6>>(
          symbols, num_values, max_entry_value, target_buffer);
    case 7:
      return EncodeRawSymbolsInternal<SymbolEncoderT<7>>(
          symbols, num_values, max_entry_value, target_buffer);
    case 8:
      return EncodeRawSymbolsInternal<SymbolEncoderT<8>>(
          symbols, num_values, max_entry_value, target_buffer);
    case 9:
      return EncodeRawSymbolsInternal<SymbolEncoderT<9>>(
          symbols, num_values, max_entry_value, target_buffer);
    case 10:
      return EncodeRawSymbolsInternal<SymbolEncoderT<10>>(
          symbols, num_values, max_entry_value, target_buffer);
    case 11:
      return EncodeRawSymbolsInternal<SymbolEncoderT<11>>(
          symbols, num_values, max_entry_value, target_buffer);
    case 12:
      return EncodeRawSymbolsInternal<SymbolEncoderT<12>>(
          symbols, num_values, max_entry_value, target_buffer);
    case 13:
      return EncodeRawSymbolsInternal<SymbolEncoderT<13>>(
          symbols, num_values, max_entry_value, target_buffer);
    case 14:
      return EncodeRawSymbolsInternal<SymbolEncoderT<14>>(
          symbols, num_values, max_entry_value, target_buffer);
    case 15:
      return EncodeRawSymbolsInternal<SymbolEncoderT<15>>(
          symbols, num_values, max_entry_value, target_buffer);
    case 16:
      return EncodeRawSymbolsInternal<SymbolEncoderT<16>>(
          symbols, num_values, max_entry_value, target_buffer);
    case 17:
      return EncodeRawSymbolsInternal<SymbolEncoderT<17>>(
          symbols, num_values, max_entry_value, target_buffer);
    case 18:
      return EncodeRawSymbolsInternal<SymbolEncoderT<18>>(
          symbols, num_values, max_entry_value, target_buffer);
    default:
      return false;
  }
}

}  // namespace draco
