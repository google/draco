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
// File providing shared funcionality for RAnsSymbolEncoder and
// RAnsSymbolDecoder (see rans_symbol_encoder.h / rans_symbol_decoder.h).
#ifndef DRACO_CORE_RANS_SYMBOL_CODING_H_
#define DRACO_CORE_RANS_SYMBOL_CODING_H_

#include "core/ans.h"

namespace draco {

// Computes the desired precision of the rANS method for the specified maximal
// symbol bit length of the input data.
constexpr int ComputeRAnsUnclampedPrecision(int max_bit_length) {
  return (3 * max_bit_length) / 2;
}

// Computes the desired precision clamped to guarantee a valid funcionality of
// our rANS library (which is between 12 to 20 bits).
constexpr int ComputeRAnsPrecisionFromMaxSymbolBitLength(int max_bit_length) {
  return ComputeRAnsUnclampedPrecision(max_bit_length) < 12
             ? 12
             : ComputeRAnsUnclampedPrecision(max_bit_length) > 20
                   ? 20
                   : ComputeRAnsUnclampedPrecision(max_bit_length);
}

}  // namespace draco

#endif  // DRACO_CORE_RANS_SYMBOL_CODING_H_
