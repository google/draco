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
#include "core/direct_bit_coding.h"
#include <iostream>

namespace draco {

DirectBitEncoder::DirectBitEncoder() : local_bits_(0), num_local_bits_(0) {}

DirectBitEncoder::~DirectBitEncoder() { Clear(); }

void DirectBitEncoder::StartEncoding() { Clear(); }

void DirectBitEncoder::EndEncoding(EncoderBuffer *target_buffer) {
  bits_.push_back(local_bits_);
  const uint32_t size_in_byte = bits_.size() * 4;
  target_buffer->Encode(size_in_byte);
  target_buffer->Encode(bits_.data(), size_in_byte);
  Clear();
}

void DirectBitEncoder::Clear() {
  bits_.clear();
  local_bits_ = 0;
  num_local_bits_ = 0;
}

DirectBitDecoder::DirectBitDecoder() : pos_(bits_.end()), num_used_bits_(0) {}

DirectBitDecoder::~DirectBitDecoder() { Clear(); }

bool DirectBitDecoder::StartDecoding(DecoderBuffer *source_buffer) {
  Clear();
  uint32_t size_in_bytes;
  if (!source_buffer->Decode(&size_in_bytes))
    return false;
  if (size_in_bytes > source_buffer->remaining_size())
    return false;
  bits_.resize(size_in_bytes / 4);
  if (!source_buffer->Decode(bits_.data(), size_in_bytes))
    return false;
  pos_ = bits_.begin();
  num_used_bits_ = 0;
  return true;
}

void DirectBitDecoder::Clear() {
  bits_.clear();
  num_used_bits_ = 0;
  pos_ = bits_.end();
}

}  // namespace draco
