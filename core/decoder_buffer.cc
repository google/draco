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
#include "core/decoder_buffer.h"

namespace draco {

DecoderBuffer::DecoderBuffer()
    : data_(nullptr), data_size_(0), pos_(0), bit_mode_(false) {}

void DecoderBuffer::Init(const char *data, size_t data_size) {
  data_ = data;
  data_size_ = data_size;
  pos_ = 0;
}

bool DecoderBuffer::StartBitDecoding(bool decode_size, uint64_t *out_size) {
  if (decode_size) {
    Decode(out_size);
  }
  bit_mode_ = true;
  bit_decoder_.reset(data_head(), remaining_size());
  return true;
}

void DecoderBuffer::EndBitDecoding() {
  bit_mode_ = false;
  const uint64_t bits_decoded = bit_decoder_.BitsDecoded();
  const uint64_t bytes_decoded = (bits_decoded + 7) / 8;
  pos_ += bytes_decoded;
}

}  // namespace draco
