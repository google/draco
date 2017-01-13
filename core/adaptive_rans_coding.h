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
// File provides basic classes and functions for rANS coding.
#ifndef DRACO_CORE_ADAPTIVE_RANS_CODING_H_
#define DRACO_CORE_ADAPTIVE_RANS_CODING_H_

#include <vector>

#include "core/ans.h"
#include "core/decoder_buffer.h"
#include "core/encoder_buffer.h"

namespace draco {

// Class for adaptive encoding a sequence of bits using rANS.
class AdaptiveRAnsBitEncoder {
 public:
  AdaptiveRAnsBitEncoder();
  ~AdaptiveRAnsBitEncoder();

  // Must be called before any Encode* function is called.
  void StartEncoding();

  // Encode one bit. If |bit| is true encode a 1, otherwise encode a 0.
  void EncodeBit(bool bit) { bits_.push_back(bit); }

  // Encode |nibts| of |value|, starting from the least significant bit.
  // |nbits| must be > 0 and <= 32.
  void EncodeLeastSignificantBits32(int nbits, uint32_t value) {
    DCHECK_EQ(true, nbits <= 32);
    DCHECK_EQ(true, nbits > 0);
    uint32_t selector = (1 << (nbits - 1));
    while (selector) {
      EncodeBit(value & selector);
      selector = selector >> 1;
    }
  }

  // Ends the bit encoding and stores the result into the target_buffer.
  void EndEncoding(EncoderBuffer *target_buffer);

 private:
  void Clear();

  std::vector<bool> bits_;
};

// Class for decoding a sequence of bits that were encoded with
// AdaptiveRAnsBitEncoder.
class AdaptiveRAnsBitDecoder {
 public:
  AdaptiveRAnsBitDecoder();
  ~AdaptiveRAnsBitDecoder();

  // Sets |source_buffer| as the buffer to decode bits from.
  void StartDecoding(DecoderBuffer *source_buffer);

  // Decode one bit. Returns true if the bit is a 1, otherwsie false.
  bool DecodeNextBit();

  // Decode the next |nbits| and return the sequence in |value|. |nbits| must be
  // > 0 and <= 32.
  void DecodeLeastSignificantBits32(int nbits, uint32_t *value);

  void EndDecoding() {}

 private:
  void Clear();

  AnsDecoder ans_decoder_;
  double p0_f_;
};

}  // namespace draco

#endif  // DRACO_CORE_ADAPTIVE_RANS_CODING_H_
