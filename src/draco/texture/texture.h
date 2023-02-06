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
#ifndef DRACO_TEXTURE_TEXTURE_H_
#define DRACO_TEXTURE_TEXTURE_H_

#include "draco/draco_features.h"

#ifdef DRACO_TRANSCODER_SUPPORTED
#include <memory>
#include <vector>

#include "draco/io/image_compression_options.h"
#include "draco/texture/source_image.h"

#ifdef DRACO_SIMPLIFIER_SUPPORTED
#include "draco/texture/color.h"
#include "draco/texture/color_utils.h"
#include "draco/texture/encoded_image.h"
#include "draco/texture/image.h"
#endif  // DRACO_SIMPLIFIER_SUPPORTED

namespace draco {

#ifdef DRACO_SIMPLIFIER_SUPPORTED
typedef draco::Image<RGBA> RgbaTexture;
typedef draco::Image<RGBAf> RgbafTexture;

enum class ColorFormat { RGBA, RGB, Monochrome };

// This class will first store and work with the image data in a RgbaTexture.
// As long as you don't call any of the Float functions the data will stay
// in RgbaTexture. As soon as any of the Float functions are called the
// RgbaTexture data is replicated to a RgbafTexture texture.
class Texture {
 public:
  Texture()
      : empty_rgba_pixel_(127, 127, 127, 127),
        empty_rgbaf_pixel_(0.5, 0.5, 0.5, 0.5) {}

  void Copy(const Texture &src);

  // Returns the width and height of the image. Lowercase to match Image.
  int width() const;
  int height() const;

  // Returns the pixel at |x| and |y|. These functions wrap Image<RGBA>.
  const RGBA GetPixel(int x, int y) const;
  const RGBA GetPixelUnsafe(int x, int y) const;

  // Sets the value of pixel at |x| and |y| to |rgba|. These functions wrap
  // Image<RGBA>.
  void SetPixel(int x, int y, const RGBA &rgba);
  void SetPixelUnsafe(int x, int y, const RGBA &rgba);

  // Returns the pixel at |x| and |y|. These functions wrap Image<RGBAf>.
  // If RGBAf image does not exist, the function will return the corresponding
  // dequantized RGBA pixel.
  const RGBAf GetPixelFloat(int x, int y) const;
  const RGBAf GetPixelFloatUnsafe(int x, int y) const;

  // Sets the value of pixel at |x| and |y| to |rgba|. These functions wrap
  // Image<RGBAf>.
  void SetPixelFloat(int x, int y, const RGBAf &rgbaf);
  void SetPixelFloatUnsafe(int x, int y, const RGBAf &rgbaf);

  // Templatized GetPixel* and SetPixel* operations that support both RGBA and
  // RGBAf data types.
  template <typename PixelT>
  const PixelT GetPixel(int x, int y) const;
  template <typename PixelT>
  const PixelT GetPixelUnsafe(int x, int y) const;

  template <typename PixelT>
  const void SetPixel(int x, int y, const PixelT &rgba);
  template <typename PixelT>
  const void SetPixelUnsafe(int x, int y, const PixelT &rgba);

  // Fills the texture with |color|.
  void FillImage(RGBA color);

  // Returns number of pixels in the image.
  int NumberOfPixels() const;

  // Resize the RGBA image. If the RGBAf image is not null then resize that
  // image.
  void Resize(int width, int height);

  // Assign |rgba| to our RGBA iamge. If our RGBAf image is not null then copy
  // |rgba| to our RGBAf image.
  void MoveImage(std::unique_ptr<RgbaTexture> rgba);

  // Copies data in |rgba_| to |rgbaf_| only if both textures are initialized.
  void CopyRGBAToRGBAf();

  // Copies data in |rgbaf_| to |rgba_| only if both textures are initialized.
  void CopyRGBAfToRGBA();

  // Returns true if the RGBA data is contiguous.
  bool IsDataContiguousRGBA() const;

  // Returns the size of the RGBA image data.
  int DataSizeRGBA() const;

  // Returns a non-mutable Image<RGBA>.
  const RgbaTexture &GetRGBAImage() const { return *rgba_; }

  // Returns a mutable Image<RGBA>. Use with caution because if |RgbaTexture|
  // is modified then the modifications will not be automatically replicated in
  // |rgbaf_|. If the |RgbaTexture| data is modified, the caller should also
  // call CopyRGBAToRGBAf().
  RgbaTexture *GetMutableRGBAImage();

  // Returns a pointer to the first RGBA pixel. Use with caution because if
  // |RGBA| is modified then the modifications will not be automatically
  // replicated in |rgbaf_|. If the |RGBA| data is modified, the caller should
  // also call CopyRGBAToRGBAf().
  RGBA *GetMutableRGBADataUnsafe();

  // Returns a pointer to the first RGBAf pixel. Use with caution because if
  // |RGBAf| is modified then the modifications will not be automatically
  // replicated in |rgba_|. If the |RGBAf| data is modified, the caller should
  // also call CopyRGBAfToRGBA().
  RGBAf *GetMutableRGBAfDataUnsafe();

  void set_source_image(const SourceImage &image) { source_image_.Copy(image); }
  const SourceImage &source_image() const { return source_image_; }
  SourceImage &source_image() { return source_image_; }

  // Getters for encoded images. The caller is responsible for limiting the
  // lifespan of encoded images to the lifespan of one simplification module. No
  // encoded images should be passed between the modules.
  typedef std::vector<std::unique_ptr<EncodedImage>> EncodedImages;
  const EncodedImages &GetEncodedImages() const { return encoded_images_; }
  EncodedImages &GetMutableEncodedImages() { return encoded_images_; }

  void SetCompressionOptions(const ImageCompressionOptions &options) {
    compression_options_ = options;
  }
  const ImageCompressionOptions &GetCompressionOptions() const {
    return compression_options_;
  }
  ImageCompressionOptions &GetMutableCompressionOptions() {
    return compression_options_;
  }

 private:
  // Creates and copies data in |rgba_| to |rgbaf_|.
  void ReplicateRGBAToRGBAf();

  std::unique_ptr<RgbaTexture> rgba_;
  std::unique_ptr<RgbafTexture> rgbaf_;

  // Returns these pixel values if |rgba_| or |rgbaf_| has not been created.
  RGBA empty_rgba_pixel_;
  RGBAf empty_rgbaf_pixel_;

  // If set this is the image that this texture is based from.
  SourceImage source_image_;

  // Vector may contain texture images encoded in various image formats and with
  // various compression settings. See draco::TextureImageEncoder methods for
  // operating on encoded images. When the uncompressed texture image is
  // modified and encoded images become invalid, it is responsibility of class
  // user to remove them from the vector.
  EncodedImages encoded_images_;

  // Compression options for this texture.
  ImageCompressionOptions compression_options_;
};

#else

// Texture class storing the source image data.
class Texture {
 public:
  void Copy(Texture &other) { source_image_.Copy(other.source_image_); }

  void set_source_image(const SourceImage &image) { source_image_.Copy(image); }
  const SourceImage &source_image() const { return source_image_; }
  SourceImage &source_image() { return source_image_; }

 private:
  // If set this is the image that this texture is based from.
  SourceImage source_image_;
};
#endif  // DRACO_SIMPLIFIER_SUPPORTED

}  // namespace draco

#endif  // DRACO_TRANSCODER_SUPPORTED
#endif  // DRACO_TEXTURE_TEXTURE_H_
