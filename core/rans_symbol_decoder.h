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
#ifndef DRACO_CORE_RANS_SYMBOL_DECODER_H_
#define DRACO_CORE_RANS_SYMBOL_DECODER_H_

#include "core/decoder_buffer.h"
#include "core/rans_symbol_coding.h"

namespace draco {

// A helper class for decoding symbols using the rANS algorithm (see ans.h).
// The class can be used to decode the probability table and the data encoded
// by the RAnsSymbolEncoder. |max_symbol_bit_length_t| must be the same as the
// one used for the corresponding RAnsSymbolEncoder.
template <int max_symbol_bit_length_t>
class RAnsSymbolDecoder {
 public:
  RAnsSymbolDecoder() : num_symbols_(0) {}

  // Initialize the decoder and decode the probability table.
  bool Create(DecoderBuffer *buffer);

  uint32_t num_symbols() const { return num_symbols_; }

  // Starts decoding from the buffer. The buffer will be advanced past the
  // encoded data after this call.
  void StartDecoding(DecoderBuffer *buffer);
  uint32_t DecodeSymbol() { return ans_.rans_read(); }
  void EndDecoding();

 private:
  static constexpr int max_symbols_ = 1 << max_symbol_bit_length_t;
  static constexpr int rans_precision_bits_ =
      ComputeRAnsPrecisionFromMaxSymbolBitLength(max_symbol_bit_length_t);
  static constexpr int rans_precision_ = 1 << rans_precision_bits_;

  std::vector<uint32_t> probability_table_;
  uint32_t num_symbols_;
  RAnsDecoder<rans_precision_bits_> ans_;
};

template <int max_symbol_bit_length_t>
bool RAnsSymbolDecoder<max_symbol_bit_length_t>::Create(DecoderBuffer *buffer) {
  // Decode the number of alphabet symbols.
  if (!buffer->Decode(&num_symbols_))
    return false;
  probability_table_.resize(num_symbols_);
  if (num_symbols_ == 0)
    return true;
  // Decode the table.
  for (uint32_t i = 0; i < num_symbols_; ++i) {
    uint32_t prob = 0;
    uint8_t byte_prob = 0;
    // Decode the first byte and extract the number of extra bytes we need to
    // get.
    if (!buffer->Decode(&byte_prob))
      return false;
    const int extra_bytes = byte_prob & 3;
    prob = byte_prob >> 2;
    for (int b = 0; b < extra_bytes; ++b) {
      uint8_t eb;
      if (!buffer->Decode(&eb))
        return false;
      // Shift 8 bits for each extra byte and subtract 2 for the two first bits.
      prob |= static_cast<uint32_t>(eb) << (8 * (b + 1) - 2);
    }
    probability_table_[i] = prob;
  }
  if (!ans_.rans_build_look_up_table(&probability_table_[0], num_symbols_))
    return false;
  return true;
}

template <int max_symbol_bit_length_t>
void RAnsSymbolDecoder<max_symbol_bit_length_t>::StartDecoding(
    DecoderBuffer *buffer) {
  uint64_t bytes_encoded;
  // Decode the number of bytes encoded by the encoder.
  buffer->Decode(&bytes_encoded);
  const uint8_t *const data_head =
      reinterpret_cast<const uint8_t *>(buffer->data_head());
  // Advance the buffer past the rANS data.
  buffer->Advance(bytes_encoded);
  ans_.read_init(data_head, bytes_encoded);
}

template <int max_symbol_bit_length_t>
void RAnsSymbolDecoder<max_symbol_bit_length_t>::EndDecoding() {
  ans_.read_end();
}

}  // namespace draco

#endif  // DRACO_CORE_RANS_SYMBOL_DECODER_H_
