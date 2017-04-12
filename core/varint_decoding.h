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
#ifndef DRACO_CORE_VARINT_DECODING_H_
#define DRACO_CORE_VARINT_DECODING_H_

#include <type_traits>

#include "core/decoder_buffer.h"
#include "core/symbol_decoding.h"

namespace draco {

// Decodes a specified integer as varint. Note that the IntTypeT must be the
// same as the one used in the corresponding EncodeVarint() call.
template <typename IntTypeT>
void DecodeVarint(IntTypeT *out_val, DecoderBuffer *buffer) {
  if (std::is_unsigned<IntTypeT>::value) {
    // Coding of unsigned values.
    // 0-6 bit - data
    // 7 bit - next byte?
    uint8_t in;
    buffer->Decode(&in);
    if (in & (1 << 7)) {
      // Next byte is available, decode it first.
      DecodeVarint<IntTypeT>(out_val, buffer);
      // Append decoded info from this byte.
      *out_val <<= 7;
      *out_val |= in & ((1 << 7) - 1);
    } else {
      // Last byte reached
      *out_val = in;
    }
  } else {
    // IntTypeT is a signed value. Decode the symbol and convert to signed.
    typename std::make_unsigned<IntTypeT>::type symbol;
    DecodeVarint(&symbol, buffer);
    *out_val = ConvertSymbolToSignedInt(symbol);
  }
}

}  // namespace draco

#endif  // DRACO_CORE_VARINT_DECODING_H_
