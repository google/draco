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
#ifndef DRACO_CORE_SYMBOL_ENCODING_H_
#define DRACO_CORE_SYMBOL_ENCODING_H_

#include <type_traits>

#include "core/encoder_buffer.h"

namespace draco {

// Helper function that converts signed integer values into unsigned integer
// symbols that can be encoded using an entropy encoder.
void ConvertSignedIntsToSymbols(const int32_t *in, int in_values,
                                uint32_t *out);

// Helper function that converts a single signed integer value into an unsigned
// integer symbol that can be encoded using an entropy encoder.
template <class IntTypeT>
typename std::make_unsigned<IntTypeT>::type ConvertSignedIntToSymbol(
    IntTypeT val) {
  typedef typename std::make_unsigned<IntTypeT>::type UnsignedType;
  static_assert(std::is_integral<IntTypeT>::value, "IntTypeT is not integral.");
  // Early exit if val is positive.
  if (val >= 0) {
    return static_cast<UnsignedType>(val) << 1;
  }
  val = -(val + 1);  // Map -1 to 0, -2 to -1, etc..
  UnsignedType ret = static_cast<UnsignedType>(val);
  ret <<= 1;
  ret |= 1;
  return ret;
}

// Encodes an array of symbols using an entropy coding. This function
// automatically decides whether to encode the symbol values using using bit
// length tags (see EncodeTaggedSymbols), or whether to encode them directly
// (see EncodeRawSymbols). The symbols can be grouped into separate components
// that can be used for better compression.
// Returns false on error.
bool EncodeSymbols(const uint32_t *symbols, int num_values, int num_components,
                   EncoderBuffer *target_buffer);

}  // namespace draco

#endif  // DRACO_CORE_SYMBOL_ENCODING_H_
