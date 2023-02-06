// Copyright 2019 The Draco Authors.
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
#ifndef DRACO_TEXTURE_TEXTURE_UTILS_H_
#define DRACO_TEXTURE_TEXTURE_UTILS_H_

#include "draco/draco_features.h"

#ifdef DRACO_TRANSCODER_SUPPORTED
#include "draco/core/status.h"
#include "draco/core/status_or.h"
#include "draco/io/file_utils.h"
#include "draco/material/material_library.h"
#include "draco/texture/texture_library.h"
#include "draco/texture/texture_map.h"

namespace draco {

// Helper class implementing various utilities operating on draco::Texture.
class TextureUtils {
 public:
#ifdef DRACO_SIMPLIFIER_SUPPORTED
  // Returns true if the |texture| is fully opaque (no transparent pixels).
  static bool IsTextureOpaque(const Texture &texture);

  // Returns true if the |texture| is fully transparent.
  static bool IsTextureFullyTransparent(const Texture &texture);

  // Resamples an input texture |in_texture| into |out_texture| using the given
  // target resolution. Resampled values are computed using the bilinear
  // interpolation scheme.
  static Status ResampleTexture(const Texture &in_texture, int target_width,
                                int target_height,
                                TextureMap::WrappingMode wrapping_mode,
                                Texture *out_texture);

  // Resizes |in_texture| to the target resolution. Unlike ResampleTexture(),
  // this function may resample the input texture repeatedly in case the source
  // and target resolutions are sufficiently different.
  static Status ResizeTexture(const Texture &in_texture, int target_width,
                              int target_height,
                              TextureMap::WrappingMode wrapping_mode,
                              Texture *out_texture);

  // Checks if two textures are equivalent.
  static bool AreTexturesEquivalent(const Texture &texture_a,
                                    const Texture &texture_b);

  // Checks if two textures are similar, with acceptable differences up to
  // |tolerance| in each pixel channel.
  static bool AreTexturesSimilar(const Texture &texture_a,
                                 const Texture &texture_b, int tolerance);

  static StatusOr<std::unique_ptr<Texture>> CreateTexture(
      const unsigned char *data, int width, int height, int num_channels,
      int bitdepth);

  // Returns a hash of the RGBA data in |texture|. The value returned will never
  // be 0.
  // TODO(vytyaz): Consider storing the hash in draco::Texture and invalidating
  //               or recomputing it when the texture image changes.
  static StatusOr<uint64_t> HashTexture(const Texture &texture);

  // Selectively checks pixel components of the texture. Returns true when
  // selected pixel components in |texture| are all identical.
  static bool TexturePixelComponentsEqual(const Texture &texture,
                                          bool check_red, bool check_green,
                                          bool check_blue, bool check_alpha);
#endif  // DRACO_SIMPLIFIER_SUPPORTED

  // Returns |texture| image stem (file basename without extension) based on the
  // source image filename or an empty string when source image is not set.
  static std::string GetTargetStem(const Texture &texture);

  // Returns |texture| image stem (file basename without extension) based on the
  // source image filename or a name generated from |index| and |suffix| like
  // "Texture5_BaseColor" when source image is not set.
  static std::string GetOrGenerateTargetStem(const Texture &texture, int index,
                                             const std::string &suffix);

  // Returns |texture| format based on compression settings, the source image
  // mime type or the source image filename.
  static ImageFormat GetTargetFormat(const Texture &texture);

  // Returns |texture| image file extension based on compression settings, the
  // source image mime type or the source image filename.
  static std::string GetTargetExtension(const Texture &texture);

  // Returns |texture| format based on source image mime type or the source
  // image filename.
  static ImageFormat GetSourceFormat(const Texture &texture);

  // Returns image format corresponding to a given image file |extension|. NONE
  // is returned when |extension| is empty or unknown.
  static ImageFormat GetFormat(const std::string &extension);

  // Returns image file extension corresponding to a given image |format|. Empty
  // extension is returned when the |format| is NONE.
  static std::string GetExtension(ImageFormat format);

#ifdef DRACO_SIMPLIFIER_SUPPORTED
  // Sets given compression |options| to all textures in |texture_library|.
  static void SetCompressionOptions(const ImageCompressionOptions &options,
                                    TextureLibrary *texture_library);

  // Sets given compression |options| to all textures of |texture_type| in
  // |material_library|.
  static void SetCompressionOptions(const ImageCompressionOptions &options,
                                    TextureMap::Type texture_type,
                                    MaterialLibrary *material_library);

  // Sets a given image compression |speed| to all textures in |texture_library|
  // without changing any other image compression settings.
  static void SetCompressionSpeed(const ImageCompressionSpeed &speed,
                                  TextureLibrary *texture_library);

  // Returns a boolean indicating whether any texture in the |texture_library|
  // is configured for compression in a given image |format|.
  static bool HasTargetImageFormat(const TextureLibrary &texture_library,
                                   ImageFormat format);

  // Return a vector of all textures from |material_library|.
  static std::vector<Texture *> FindMutableTextures(
      MaterialLibrary *material_library);

  // Return a vector of textures of |texture_type| from |material_library|.
  static std::vector<Texture *> FindMutableTextures(
      const TextureMap::Type texture_type, MaterialLibrary *material_library);
#endif  // DRACO_SIMPLIFIER_SUPPORTED

  // Returns the number of channels required for encoding a |texture| from a
  // given |material_library|, taking into account texture opacity and assuming
  // that occlusion and metallic-roughness texture maps may share a texture.
  // TODO(vytyaz): Move this and FindTextures() to MaterialLibrary class.
  static int ComputeRequiredNumChannels(
      const Texture &texture, const MaterialLibrary &material_library);

  static std::vector<const Texture *> FindTextures(
      const TextureMap::Type texture_type,
      const MaterialLibrary *material_library);
};

}  // namespace draco

#endif  // DRACO_TRANSCODER_SUPPORTED
#endif  // DRACO_TEXTURE_TEXTURE_UTILS_H_
