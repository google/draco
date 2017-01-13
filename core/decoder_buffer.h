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
#ifndef DRACO_CORE_DECODER_BUFFER_H_
#define DRACO_CORE_DECODER_BUFFER_H_

#include <cstdint>
#include <cstring>
#include <memory>

#include "core/bit_coder.h"

namespace draco {

// Class is a wrapper around input data used by MeshDecoder. It provides a
// basic interface for decoding either typed or variable-bit sized data.
class DecoderBuffer {
 public:
  DecoderBuffer();
  DecoderBuffer(const DecoderBuffer &buf) = default;

  DecoderBuffer &operator=(const DecoderBuffer &buf) = default;

  // Sets the buffer's internal data. Note that no copy of the input data is
  // made so the data owner needs to keep the data valid and unchaged for
  // runtime of the decoder.
  void Init(const char *data, size_t data_size);

  // Starts decoding a bit sequence.
  // decode_size must be true if the size of the encoded bit data was included,
  // during encoding. The size is then returned to out_size.
  // Returns false on error.
  bool StartBitDecoding(bool decode_size, uint64_t *out_size);

  // Ends the decoding of the bit sequence and return to the default
  // byte-aligned decoding.
  void EndBitDecoding();

  // Decodes up to 32 bits into out_val. Can be called only in between
  // StartBitDecoding and EndBitDeoding. Otherwise returns false.
  bool DecodeLeastSignificantBits32(int nbits, uint32_t *out_value) {
    if (!bit_decoder_active())
      return false;
    bit_decoder_.GetBits(nbits, out_value);
    return true;
  }

  // Decodes an arbitrary data type.
  // Can be used only when we are not decoding a bit-sequence.
  // Returns false on error.
  template <typename T>
  bool Decode(T *out_val) {
    if (!Peek(out_val))
      return false;
    pos_ += sizeof(T);
    return true;
  }

  bool Decode(void *out_data, size_t size_to_decode) {
    if (data_size_ < static_cast<int64_t>(pos_ + size_to_decode))
      return false;  // Buffer overflow.
    memcpy(out_data, (data_ + pos_), size_to_decode);
    pos_ += size_to_decode;
    return true;
  }

  // Decodes an arbitrary data, but does not advance the reading position.
  template <typename T>
  bool Peek(T *out_val) {
    const size_t size_to_decode = sizeof(T);
    if (data_size_ < static_cast<int64_t>(pos_ + size_to_decode))
      return false;  // Buffer overflow.
    memcpy(out_val, (data_ + pos_), size_to_decode);
    return true;
  }

  bool Peek(void *out_data, size_t size_to_peek) {
    if (data_size_ < static_cast<int64_t>(pos_ + size_to_peek))
      return false;  // Buffer overflow.
    memcpy(out_data, (data_ + pos_), size_to_peek);
    return true;
  }

  // Discards #bytes from the input buffer.
  void Advance(int64_t bytes) { pos_ += bytes; }

  // Moves the parsing position to a specific offset from the beggining of the
  // input data.
  void StartDecodingFrom(int64_t offset) { pos_ = offset; }

  // Returns the data array at the current decoder position.
  const char *data_head() const { return data_ + pos_; }
  int64_t remaining_size() const { return data_size_ - pos_; }
  int64_t decoded_size() const { return pos_; }
  BitDecoder *bit_decoder() { return &bit_decoder_; }
  bool bit_decoder_active() const { return bit_mode_; }

 private:
  const char *data_;
  int64_t data_size_;

  // Current parsing position of the decoder.
  int64_t pos_;
  BitDecoder bit_decoder_;
  bool bit_mode_;
};

}  // namespace draco

#endif  // DRACO_CORE_DECODER_BUFFER_H_
