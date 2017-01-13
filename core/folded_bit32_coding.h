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
// File provides direct encoding of bits with arithmetic encoder interface.
#ifndef DRACO_CORE_FOLDED_BIT32_CODING_H_
#define DRACO_CORE_FOLDED_BIT32_CODING_H_

#include <vector>

#include "core/decoder_buffer.h"
#include "core/encoder_buffer.h"

namespace draco {

// This coding scheme considers every bit of an (up to) 32bit integer as a
// separate context. This can be a significant advantage when encoding numbers
// where it is more likely that the front bits are zero.
// The behavior is essentially the same as other arithmetic encoding schemes,
// the only difference is that encoding and decoding of bits must be absolutely
// symmetric, bits handed in by EncodeBit32 must be also decoded in this way.
template <class BitEncoderT>
class FoldedBit32Encoder {
 public:
  FoldedBit32Encoder() {}
  ~FoldedBit32Encoder() {}

  // Must be called before any Encode* function is called.
  void StartEncoding() {
    for (int i = 0; i < 32; i++) {
      folded_number_encoders_[i].StartEncoding();
    }
    bit_encoder_.StartEncoding();
  }

  // Encode one bit. If |bit| is true encode a 1, otherwise encode a 0.
  void EncodeBit(bool bit) { bit_encoder_.EncodeBit(bit); }

  // Encode |nbits| of |value|, starting from the least significant bit.
  // |nbits| must be > 0 and <= 32.
  void EncodeLeastSignificantBits32(int nbits, uint32_t value) {
    uint32_t selector = 1 << (nbits - 1);
    for (int i = 0; i < nbits; i++) {
      const bool bit = (value & selector);
      folded_number_encoders_[i].EncodeBit(bit);
      selector = selector >> 1;
    }
  }

  // Ends the bit encoding and stores the result into the target_buffer.
  void EndEncoding(EncoderBuffer *target_buffer) {
    for (int i = 0; i < 32; i++) {
      folded_number_encoders_[i].EndEncoding(target_buffer);
    }
    bit_encoder_.EndEncoding(target_buffer);
  }

 private:
  void Clear() {
    for (int i = 0; i < 32; i++) {
      folded_number_encoders_[i].Clear();
    }
    bit_encoder_.Clear();
  }

  std::array<BitEncoderT, 32> folded_number_encoders_;
  BitEncoderT bit_encoder_;
};

template <class BitDecoderT>
class FoldedBit32Decoder {
 public:
  FoldedBit32Decoder() {}
  ~FoldedBit32Decoder() {}

  // Sets |source_buffer| as the buffer to decode bits from.
  void StartDecoding(DecoderBuffer *source_buffer) {
    for (int i = 0; i < 32; i++) {
      folded_number_decoders_[i].StartDecoding(source_buffer);
    }
    bit_decoder_.StartDecoding(source_buffer);
  }

  // Decode one bit. Returns true if the bit is a 1, otherwise false.
  bool DecodeNextBit() { return bit_decoder_.DecodeNextBit(); }

  // Decode the next |nbits| and return the sequence in |value|. |nbits| must be
  // > 0 and <= 32.
  void DecodeLeastSignificantBits32(int nbits, uint32_t *value) {
    uint32_t result = 0;
    for (int i = 0; i < nbits; ++i) {
      const bool bit = folded_number_decoders_[i].DecodeNextBit();
      result = (result << 1) + bit;
    }
    *value = result;
  }

  void EndDecoding() {
    for (int i = 0; i < 32; i++) {
      folded_number_decoders_[i].EndDecoding();
    }
    bit_decoder_.EndDecoding();
  }

 private:
  void Clear() {
    for (int i = 0; i < 32; i++) {
      folded_number_decoders_[i].Clear();
    }
    bit_decoder_.Clear();
  }

  std::array<BitDecoderT, 32> folded_number_decoders_;
  BitDecoderT bit_decoder_;
};

}  // namespace draco

#endif  // DRACO_CORE_FOLDED_BIT32_CODING_H_
