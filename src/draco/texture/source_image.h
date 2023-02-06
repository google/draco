// Copyright 2021 The Draco Authors.
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
#ifndef DRACO_TEXTURE_SOURCE_IMAGE_H_
#define DRACO_TEXTURE_SOURCE_IMAGE_H_

#include "draco/draco_features.h"

#ifdef DRACO_TRANSCODER_SUPPORTED
#include <cstdint>
#include <string>
#include <vector>

#include "draco/core/status.h"

namespace draco {

// This class is used to hold the encoded and decoded data and characteristics
// for an image. In order for the image to contain "valid" encoded data, either
// the |filename_| must point to a valid image file or the |mime_type_| and
// |encoded_data_| must contain valid image data.
class SourceImage {
 public:
#ifdef DRACO_SIMPLIFIER_SUPPORTED
  SourceImage() : width_(0), height_(0), bit_depth_(0), decoded_data_hash_(0) {}
#else
  SourceImage() {}
#endif

  // No copy constructors.
  SourceImage(const SourceImage &) = delete;
  SourceImage &operator=(const SourceImage &) = delete;
  // No move constructors.
  SourceImage(SourceImage &&) = delete;
  SourceImage &operator=(SourceImage &&) = delete;

  void Copy(const SourceImage &src);

#ifdef DRACO_SIMPLIFIER_SUPPORTED
  int width() const { return width_; }
  void set_width(int width) { width_ = width; }
  int height() const { return height_; }
  void set_height(int height) { height_ = height; }
  int bit_depth() const { return bit_depth_; }
  void set_bit_depth(int bit_depth) { bit_depth_ = bit_depth; }
#endif

  // Sets the name of the source image file.
  void set_filename(const std::string &filename) { filename_ = filename; }
  const std::string &filename() const { return filename_; }

  void set_mime_type(const std::string &mime_type) { mime_type_ = mime_type; }
  const std::string &mime_type() const { return mime_type_; }

  std::vector<uint8_t> &MutableEncodedData() { return encoded_data_; }
  const std::vector<uint8_t> &encoded_data() const { return encoded_data_; }

#ifdef DRACO_SIMPLIFIER_SUPPORTED
  uint64_t decoded_data_hash() const { return decoded_data_hash_; }
  void set_decoded_data_hash(uint64_t hash) { decoded_data_hash_ = hash; }
#endif

 private:
#ifdef DRACO_SIMPLIFIER_SUPPORTED
  int width_;
  int height_;
  int bit_depth_;
#endif

  // The filename of the image. This field can be empty as long as |mime_type_|
  // and |encoded_data_| is not empty.
  std::string filename_;

  // The mimetype of the |encoded_data_|.
  std::string mime_type_;

  // The encoded data of the image. This field can be empty as long as
  // |filename_| is not empty.
  std::vector<uint8_t> encoded_data_;

#ifdef DRACO_SIMPLIFIER_SUPPORTED
  // The hash of the decoded image data. The hash must be generated with
  // TextureUtils::HashTexture(), which is guaranteed to never return 0. A value
  // of 0 signifies that a hash has not been set.
  uint64_t decoded_data_hash_;
#endif
};

}  // namespace draco

#endif  // DRACO_TRANSCODER_SUPPORTED
#endif  // DRACO_TEXTURE_SOURCE_IMAGE_H_
