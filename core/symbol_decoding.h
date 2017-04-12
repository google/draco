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
#ifndef DRACO_CORE_SYMBOL_DECODING_H_
#define DRACO_CORE_SYMBOL_DECODING_H_

#include "core/decoder_buffer.h"

namespace draco {

// Converts unsigned integer symbols encoded with an entropy encoder back to
// signed values.
void ConvertSymbolsToSignedInts(const uint32_t *in, int in_values,
                                int32_t *out);

// Converts a single unsigned integer symbol encoded with an entropy encoder
// back to a signed value.
template <class IntTypeT>
typename std::make_signed<IntTypeT>::type ConvertSymbolToSignedInt(
    IntTypeT val) {
  static_assert(std::is_integral<IntTypeT>::value, "IntTypeT is not integral.");
  typedef typename std::make_signed<IntTypeT>::type SignedType;
  const bool is_positive = !static_cast<bool>(val & 1);
  val >>= 1;
  if (is_positive) {
    return static_cast<SignedType>(val);
  }
  SignedType ret = static_cast<SignedType>(val);
  ret = -ret - 1;
  return ret;
}

// Decodes an array of symbols that was previously encoded with an entropy code.
// Returns false on error.
bool DecodeSymbols(int num_values, int num_components,
                   DecoderBuffer *src_buffer, uint32_t *out_values);

}  // namespace draco

#endif  // DRACO_CORE_SYMBOL_DECODING_H_
