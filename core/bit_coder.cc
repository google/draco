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
#include "core/bit_coder.h"

// TODO(fgalligan): Remove this file.
namespace draco {

BitEncoder::BitEncoder(char *data) : bit_buffer_(data), bit_offset_(0) {}

BitDecoder::BitDecoder()
    : bit_buffer_(nullptr), bit_buffer_end_(nullptr), bit_offset_(0) {}

BitDecoder::~BitDecoder() {}

}  // namespace draco
