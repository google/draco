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
#ifndef DRACO_CORE_BIT_CODER_H_
#define DRACO_CORE_BIT_CODER_H_

#include <cassert>
#include <cstddef>
#include <cstdint>

#include "core/bit_utils.h"
#include "core/macros.h"

namespace draco {

// Class to encode bits to a bit buffer.
class BitEncoder {
 public:
  // |data| is the buffer to write the bits into.
  explicit BitEncoder(char *data);

  // Write |nbits| of |data| into the bit buffer.
  void PutBits(uint32_t data, int32_t nbits) {
    DCHECK_GE(nbits, 0);
    DCHECK_LE(nbits, 32);
    for (int32_t bit = 0; bit < nbits; ++bit)
      PutBit((data >> bit) & 1);
  }

  // Return number of bits encoded so far.
  uint64_t Bits() const { return static_cast<uint64_t>(bit_offset_); }

  // TODO(fgalligan): Remove this function once we know we do not need the
  // old API anymore.
  // This is a function of an old API, that currently does nothing.
  void Flush(int /* left_over_bit_value */) {}

  // Return the number of bits required to store the given number
  static uint32_t BitsRequired(uint32_t x) {
    return static_cast<uint32_t>(bits::MostSignificantBit(x));
  }

 private:
  void PutBit(uint8_t value) {
    const int byte_size = 8;
    const uint64_t off = static_cast<uint64_t>(bit_offset_);
    const uint64_t byte_offset = off / byte_size;
    const int bit_shift = off % byte_size;

    // TODO(fgalligan): Check performance if we add a branch and only do one
    // memory write if bit_shift is 7. Also try using a temporary variable to
    // hold the bits before writing to the buffer.

    bit_buffer_[byte_offset] &= ~(1 << bit_shift);
    bit_buffer_[byte_offset] |= value << bit_shift;
    bit_offset_++;
  }

  char *bit_buffer_;
  size_t bit_offset_;
};

// Class to decode bits from a bit buffer.
class BitDecoder {
 public:
  BitDecoder();
  ~BitDecoder();

  // Sets the bit buffer to |b|. |s| is the size of |b| in bytes.
  inline void reset(const void *b, size_t s) {
    bit_offset_ = 0;
    bit_buffer_ = static_cast<const uint8_t *>(b);
    bit_buffer_end_ = bit_buffer_ + s;
  }

  // Returns number of bits decoded so far.
  inline uint64_t BitsDecoded() const {
    return static_cast<uint64_t>(bit_offset_);
  }

  // Return number of bits available for decoding
  inline uint64_t AvailBits() const {
    return ((bit_buffer_end_ - bit_buffer_) * 8) - bit_offset_;
  }

  inline uint32_t EnsureBits(int k) {
    DCHECK_LE(k, 24);
    DCHECK_LE(static_cast<uint64_t>(k), AvailBits());

    uint32_t buf = 0;
    for (int i = 0; i < k; ++i) {
      buf |= PeekBit(i) << i;
    }
    return buf;  // Okay to return extra bits
  }

  inline void ConsumeBits(int k) { bit_offset_ += k; }

  // Returns |nbits| bits in |x|.
  inline bool GetBits(int32_t nbits, uint32_t *x) {
    DCHECK_GE(nbits, 0);
    DCHECK_LE(nbits, 32);
    uint32_t value = 0;
    for (int32_t bit = 0; bit < nbits; ++bit)
      value |= GetBit() << bit;
    *x = value;
    return true;
  }

 private:
  // TODO(fgalligan): Add support for error reporting on range check.
  // Returns one bit from the bit buffer.
  inline int GetBit() {
    const size_t off = bit_offset_;
    const size_t byte_offset = off >> 3;
    const int bit_shift = static_cast<int>(off & 0x7);
    if (bit_buffer_ + byte_offset < bit_buffer_end_) {
      const int bit = (bit_buffer_[byte_offset] >> bit_shift) & 1;
      bit_offset_ = off + 1;
      return bit;
    }
    return 0;
  }

  inline int PeekBit(int offset) {
    const size_t off = bit_offset_ + offset;
    const size_t byte_offset = off >> 3;
    const int bit_shift = static_cast<int>(off & 0x7);
    if (bit_buffer_ + byte_offset < bit_buffer_end_) {
      const int bit = (bit_buffer_[byte_offset] >> bit_shift) & 1;
      return bit;
    }
    return 0;
  }

  const uint8_t *bit_buffer_;
  const uint8_t *bit_buffer_end_;
  size_t bit_offset_;
};

}  // namespace draco

#endif  // DRACO_CORE_BIT_CODER_H_
