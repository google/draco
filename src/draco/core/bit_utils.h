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
// File containing a basic set of bit manipulation utilities used within the
// Draco library.
// All functions are defined within its own namespace draco::bits::

#ifndef DRACO_CORE_BIT_UTILS_H_
#define DRACO_CORE_BIT_UTILS_H_

#include <stdint.h>

#if defined(_MSC_VER)
#include <intrin.h>
#endif  // defined(_MSC_VER)

namespace draco {
namespace bits {

// Returns the number of '1' bits within the input 32 bit integer.
inline int CountOnes32(uint32_t n) {
  n -= ((n >> 1) & 0x55555555);
  n = ((n >> 2) & 0x33333333) + (n & 0x33333333);
  return (((n + (n >> 4)) & 0xF0F0F0F) * 0x1010101) >> 24;
}

inline uint32_t ReverseBits32(uint32_t n) {
  n = ((n >> 1) & 0x55555555) | ((n & 0x55555555) << 1);
  n = ((n >> 2) & 0x33333333) | ((n & 0x33333333) << 2);
  n = ((n >> 4) & 0x0F0F0F0F) | ((n & 0x0F0F0F0F) << 4);
  n = ((n >> 8) & 0x00FF00FF) | ((n & 0x00FF00FF) << 8);
  return (n >> 16) | (n << 16);
}

// Copies the |nbits| from the src integer into the |dst| integer using the
// provided bit offsets |dst_offset| and |src_offset|.
inline void CopyBits32(uint32_t *dst, int dst_offset, uint32_t src,
                       int src_offset, int nbits) {
  const uint32_t mask = (~static_cast<uint32_t>(0)) >> (32 - nbits)
                                                           << dst_offset;
  *dst = (*dst & (~mask)) | (((src >> src_offset) << dst_offset) & mask);
}

// Returns the most location of the most significant bit in the input integer
// |n|.
// The functionality is not defined for |n == 0|.
inline int MostSignificantBit(uint32_t n) {
#if defined(__GNUC__)
  return 31 ^ __builtin_clz(n);
#elif defined(_MSC_VER)
  unsigned long where;
  _BitScanReverse(&where, n);
  return (int)where;
#else
  // TODO(fgalligan): Optimize this code.
  int msb = -1;
  while (n != 0) {
    msb++;
    n >>= 1;
  }
  return msb;
#endif
}

}  // namespace bits
}  // namespace draco

#endif  // DRACO_CORE_BIT_UTILS_H_
