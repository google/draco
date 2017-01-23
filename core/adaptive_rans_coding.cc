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
#include "core/adaptive_rans_coding.h"

#include <iostream>

namespace draco {

uint8_t clamp_probability(double p) {
  DCHECK_LE(p, 1.0);
  DCHECK_LE(0.0, p);
  uint32_t p_int = static_cast<uint32_t>((p * 256) + 0.5);
  p_int -= (p_int == 256);
  p_int += (p_int == 0);
  return static_cast<uint8_t>(p_int);
}

double update_probability(double old_p, bool bit) {
  static constexpr double w = 128.0;
  static constexpr double w0 = (w - 1.0) / w;
  static constexpr double w1 = 1.0 / w;
  return old_p * w0 + (!bit) * w1;
}

AdaptiveRAnsBitEncoder::AdaptiveRAnsBitEncoder() {}

AdaptiveRAnsBitEncoder::~AdaptiveRAnsBitEncoder() { Clear(); }

void AdaptiveRAnsBitEncoder::StartEncoding() { Clear(); }

void AdaptiveRAnsBitEncoder::EndEncoding(EncoderBuffer *target_buffer) {
  // Buffer for ans to write.
  std::vector<uint8_t> buffer(bits_.size() + 16);
  AnsCoder ans_coder;
  ans_write_init(&ans_coder, buffer.data());

  // Unfortunaetly we have to encode the bits in reversed order, while the
  // probabilities that should be given are those of the forward sequence.
  double p0_f = 0.5;
  std::vector<uint8_t> p0s;
  p0s.reserve(bits_.size());
  for (bool b : bits_) {
    p0s.push_back(clamp_probability(p0_f));
    p0_f = update_probability(p0_f, b);
  }
  auto bit = bits_.rbegin();
  auto pit = p0s.rbegin();
  while (bit != bits_.rend()) {
    rabs_write(&ans_coder, *bit, *pit);
    ++bit;
    ++pit;
  }

  const uint32_t size_in_bytes = ans_write_end(&ans_coder);
  target_buffer->Encode(size_in_bytes);
  target_buffer->Encode(buffer.data(), size_in_bytes);

  Clear();
}

void AdaptiveRAnsBitEncoder::Clear() { bits_.clear(); }

AdaptiveRAnsBitDecoder::AdaptiveRAnsBitDecoder() : p0_f_(0.5) {}

AdaptiveRAnsBitDecoder::~AdaptiveRAnsBitDecoder() { Clear(); }

bool AdaptiveRAnsBitDecoder::StartDecoding(DecoderBuffer *source_buffer) {
  Clear();

  uint32_t size_in_bytes;
  if (!source_buffer->Decode(&size_in_bytes))
    return false;
  if (size_in_bytes > source_buffer->remaining_size())
      return false;
  if (ans_read_init(&ans_decoder_,
                    reinterpret_cast<uint8_t *>(
                        const_cast<char *>(source_buffer->data_head())),
                    size_in_bytes) != 0)
    return false;
  source_buffer->Advance(size_in_bytes);
  return true;
}

bool AdaptiveRAnsBitDecoder::DecodeNextBit() {
  const uint8_t p0 = clamp_probability(p0_f_);
  const bool bit = static_cast<bool>(rabs_read(&ans_decoder_, p0));
  p0_f_ = update_probability(p0_f_, bit);
  return bit;
}

void AdaptiveRAnsBitDecoder::DecodeLeastSignificantBits32(int nbits,
                                                          uint32_t *value) {
  DCHECK_EQ(true, nbits <= 32);
  DCHECK_EQ(true, nbits > 0);

  uint32_t result = 0;
  while (nbits) {
    result = (result << 1) + DecodeNextBit();
    --nbits;
  }
  *value = result;
}

void AdaptiveRAnsBitDecoder::Clear() {
  ans_read_end(&ans_decoder_);
  p0_f_ = 0.5;
}

}  // namespace draco
