// Copyright 2020 The Draco Authors.
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
#ifndef DRACO_IO_IMAGE_COMPRESSION_OPTIONS_H_
#define DRACO_IO_IMAGE_COMPRESSION_OPTIONS_H_

#include "draco/draco_features.h"

#ifdef DRACO_TRANSCODER_SUPPORTED
#include <string>

namespace draco {

#ifdef DRACO_SIMPLIFIER_SUPPORTED
// Enum defining how long the texture compression should take. The quality of
// the compressed image is the inverse of the compression speed. I.e. SLOW will
// take longer to compress than NORMAL, but quality will be better. COPY is a
// special setting that should be the fastest as the texture io will try and
// copy the source image data. If COPY mode is set then any attempt to copy a
// texture that has been modified will return an error. LOSSLESS is a special
// mode that translates into COPY for lossy image codecs and NORMAL for lossless
// image codecs. LOSSLESS_SLOW is a special mode that translates into COPY for
// lossy image codecs and SLOW for lossless image codecs.
// TODO(b/155488625): Investigate changing enum to be quality based.
enum class ImageCompressionSpeed {
  SLOW,
  NORMAL,
  FAST,
  COPY,
  LOSSLESS,
  LOSSLESS_SLOW
};
#endif  // DRACO_SIMPLIFIER_SUPPORTED

// Enum defining image compression formats.
enum class ImageFormat { NONE, PNG, JPEG, BASIS, WEBP };

#ifdef DRACO_SIMPLIFIER_SUPPORTED
struct ImageCompressionOptions {
  explicit ImageCompressionOptions(ImageCompressionSpeed speed)
      : image_compression_speed(speed),
        target_image_format(ImageFormat::NONE),
        jpeg_quality(-1),
        basis_quality(128),  // TODO(vytyaz): Revisit the default value.
        generate_mipmaps(true),
        webp_quality(-1) {}
  ImageCompressionOptions()
      : ImageCompressionOptions(ImageCompressionSpeed::LOSSLESS) {}
  ImageCompressionSpeed image_compression_speed;

  // Specifies target format for compressed images. Currently supported formats
  // are JPEG, PNG, or BASIS. The NONE format indicates that the image format
  // should remain unchanged. The BASIS format indicates that the image will be
  // stored as KTX2 when the model is output as glTF.
  ImageFormat target_image_format;

  // Quality in [0, 100] range for images saved in JPEG format. When set to -1,
  // the quality will be determined based on image compression speed.
  int jpeg_quality;

  // Quality in [0, 255] range for images saved in Basis format.
  int basis_quality;

  // Indicates that mipmap levels should be generated when saving an image in
  // Basis format.
  bool generate_mipmaps;

  // Quality in [0, 100] range for images saved in WebP format. When set to -1,
  // the quality will be determined based on image compression speed.
  int webp_quality;
};
#endif  // DRACO_SIMPLIFIER_SUPPORTED

}  // namespace draco

#endif  // DRACO_TRANSCODER_SUPPORTED
#endif  // DRACO_IO_IMAGE_COMPRESSION_OPTIONS_H_
