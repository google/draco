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
#include "draco/texture/texture_utils.h"

#ifdef DRACO_TRANSCODER_SUPPORTED
#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#ifdef DRACO_SIMPLIFIER_SUPPORTED
#include "draco/core/hash_utils.h"
#include "draco/texture/texture_sampler_bilinear.h"
#endif  // DRACO_SIMPLIFIER_SUPPORTED

namespace draco {

#ifdef DRACO_SIMPLIFIER_SUPPORTED
bool AlphaEquals(const Texture &texture, uint8_t alpha) {
  for (int y = 0; y < texture.height(); ++y) {
    for (int x = 0; x < texture.width(); ++x) {
      if (texture.GetPixelUnsafe(x, y).a != alpha) {
        return false;
      }
    }
  }
  return true;
}

bool TextureUtils::IsTextureOpaque(const Texture &texture) {
  return AlphaEquals(texture, 255);
}

bool TextureUtils::IsTextureFullyTransparent(const Texture &texture) {
  return AlphaEquals(texture, 0);
}

Status TextureUtils::ResampleTexture(const Texture &in_texture,
                                     int target_width, int target_height,
                                     TextureMap::WrappingMode wrapping_mode,
                                     Texture *out_texture) {
  if (target_width <= 0 || target_height <= 0) {
    return Status(Status::DRACO_ERROR,
                  "Invalid target resolution for texture resampling.");
  }
  if (in_texture.width() == target_width &&
      in_texture.height() == target_height) {
    out_texture->Copy(in_texture);
    return OkStatus();
  }
  out_texture->Resize(target_width, target_height);

  // TODO(ostava): Add special handling for case when the target resolution is
  // half of the original resolution. This should be one of the most common
  // scenarios and it can be handled much more efficiently with simple averaging
  // rather than with generic bilinear sampling.

  for (int y = 0; y < target_height; ++y) {
    for (int x = 0; x < target_width; ++x) {
      const float u = (x + 0.5f) / target_width;
      // By convention. the image and UV spaces have a flipped y coordinate.
      const float v = 1.f - ((y + 0.5f) / target_height);
      out_texture->SetPixelUnsafe(x, y,
                                  TextureSamplerBilinear<RGBA>::Sample(
                                      in_texture, u, v, wrapping_mode));
    }
  }

  return OkStatus();
}

// Returns the next intermediate resolution based on the |current| resolution
// and the |target| resolution. If the |target| is a power of two then the
// result will also be a power of two.
int ComputeNextIntermediateResolution(int current, int target) {
  int next;
  // Check if the |target| is a power of two.
  if ((target & (target - 1)) == 0) {
    // Make the |next| resolution the greatest power of two that is less than
    // the |current| resolution.
    next = 1;
    while (2 * next < current) {
      next *= 2;
    }
  } else {
    // Make the |next| resolution half the |current| resolution.
    next = current / 2;
  }

  // Make sure the |next| resolution does not drop below the |target|.
  return std::max(next, target);
}

Status TextureUtils::ResizeTexture(const Texture &in_texture, int target_width,
                                   int target_height,
                                   TextureMap::WrappingMode wrapping_mode,
                                   Texture *out_texture) {
  int intermediate_width = in_texture.width();
  int intermediate_height = in_texture.height();
  const Texture *input_texture_ptr = &in_texture;
  std::unique_ptr<Texture> intermediate_texture;

  // Iteratively downsample the texture if needed. The intermediate resolutions
  // are either the multiples of the target resolution or the divisors of the
  // input resolution, with a factor of two, for example:
  //     200 : [100, 50] : 40
  //     200 : [128, 64] : 32
  //     256 : [128, 64] : 40
  //     256 : [128, 64] : 32
  //
  // Resampling uses bilinear interpolation and if we downsampled more
  // aggressively, not all pixels from the high-res texture would be accumulated
  // in the low-res downsampled texture. When a target resolution is a power of
  // two then the intermediate resolutions are also the powers of two.
  while (intermediate_width > target_width * 2 ||
         intermediate_height > target_height * 2) {
    // Compute the next intermediate texture resolution.
    intermediate_width =
        ComputeNextIntermediateResolution(intermediate_width, target_width);
    intermediate_height =
        ComputeNextIntermediateResolution(intermediate_height, target_height);
    // When rectangular texture is downsampled into a square texture then
    // intermediate textures are square as well.
    if (target_width == target_height &&
        intermediate_width != intermediate_height) {
      intermediate_width = std::max(intermediate_width, intermediate_height);
      intermediate_height = intermediate_width;
    }
    DRACO_RETURN_IF_ERROR(
        ResampleTexture(*input_texture_ptr, intermediate_width,
                        intermediate_height, wrapping_mode, out_texture));

    if (!intermediate_texture) {
      intermediate_texture = std::unique_ptr<Texture>(new Texture());
    }
    intermediate_texture->Copy(*out_texture);
    input_texture_ptr = intermediate_texture.get();
  }

  out_texture->set_source_image(in_texture.source_image());
  return ResampleTexture(*input_texture_ptr, target_width, target_height,
                         wrapping_mode, out_texture);
}

bool TextureUtils::AreTexturesEquivalent(const Texture &texture_a,
                                         const Texture &texture_b) {
  return AreTexturesSimilar(texture_a, texture_b, 0);
}

bool TextureUtils::AreTexturesSimilar(const Texture &texture_a,
                                      const Texture &texture_b, int tolerance) {
  if (texture_a.width() != texture_b.width()) {
    return false;
  }
  if (texture_a.height() != texture_b.height()) {
    return false;
  }
  for (int y = 0; y < texture_a.height(); ++y) {
    for (int x = 0; x < texture_a.width(); ++x) {
      const RGBA pixel_a = texture_a.GetPixelUnsafe(x, y);
      const RGBA pixel_b = texture_b.GetPixelUnsafe(x, y);
      if (!ColorUtils::AreColorsSimilar(pixel_a, pixel_b, tolerance)) {
        return false;
      }
    }
  }
  return true;
}

StatusOr<std::unique_ptr<Texture>> TextureUtils::CreateTexture(
    const unsigned char *data, int width, int height, int num_channels,
    int bitdepth) {
  if (bitdepth != 8) {
    return Status(Status::DRACO_ERROR,
                  "CreateTexture() only supports a bitdepth of 8.");
  }

  const int pixel_stride = num_channels * (bitdepth / 8);
  const int stride = width * pixel_stride;

  std::unique_ptr<Texture> texture(new Texture());
  texture->Resize(width, height);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const unsigned char *pixel_data =
          data + ((y * stride) + (pixel_stride * x));
      if (num_channels == 3) {
        const RGBA pixel(pixel_data[0], pixel_data[1], pixel_data[2], 0);
        texture->SetPixelUnsafe(x, y, pixel);
      } else if (num_channels == 4) {
        const RGBA pixel(pixel_data[0], pixel_data[1], pixel_data[2],
                         pixel_data[3]);
        texture->SetPixelUnsafe(x, y, pixel);
      } else {
        return Status(Status::DRACO_ERROR,
                      "CreateTexture() only supports channels of 3 or 4.");
      }
    }
  }
  return texture;
}

StatusOr<uint64_t> TextureUtils::HashTexture(const Texture &texture) {
  if (texture.width() == 0 || texture.height() == 0) {
    return Status(Status::DRACO_ERROR, "Texture is invalid.");
  }
  uint64_t hash = 0x87654321;
  for (int y = 0; y < texture.height(); ++y) {
    for (int x = 0; x < texture.width(); x += 2) {
      const RGBA rgba_1 = texture.GetPixelUnsafe(x, y);
      const RGBA rgba_2 = (x + 1 < texture.width())
                              ? texture.GetPixelUnsafe(x + 1, y)
                              : RGBA(173, 137, 65, 43);
      const uint64_t value = static_cast<uint64_t>(rgba_2.r) << 56 |
                             static_cast<uint64_t>(rgba_2.g) << 48 |
                             static_cast<uint64_t>(rgba_2.b) << 40 |
                             static_cast<uint64_t>(rgba_2.a) << 32 |
                             rgba_1.r << 24 | rgba_1.g << 16 | rgba_1.b << 8 |
                             rgba_1.a;
      hash = HashCombine(hash, value);
    }
  }
  if (hash < std::numeric_limits<uint64_t>::max()) {
    hash += 1;
  }
  return hash;
}

bool TextureUtils::TexturePixelComponentsEqual(const Texture &texture,
                                               bool check_red, bool check_green,
                                               bool check_blue,
                                               bool check_alpha) {
  const RGBA first_pixel = texture.GetPixelUnsafe(0, 0);
  for (int y = 0; y < texture.height(); ++y) {
    for (int x = 0; x < texture.width(); ++x) {
      const RGBA pixel = texture.GetPixelUnsafe(x, y);
      if (check_red && pixel.r != first_pixel.r) {
        return false;
      }
      if (check_green && pixel.g != first_pixel.g) {
        return false;
      }
      if (check_blue && pixel.b != first_pixel.b) {
        return false;
      }
      if (check_alpha && pixel.a != first_pixel.a) {
        return false;
      }
    }
  }
  return true;
}
#endif  // DRACO_SIMPLIFIER_SUPPORTED

std::string TextureUtils::GetTargetStem(const Texture &texture) {
  // Return stem of the source image if there is one.
  if (!texture.source_image().filename().empty()) {
    const std::string &full_path = texture.source_image().filename();
    std::string folder_path;
    std::string filename;
    SplitPath(full_path, &folder_path, &filename);
    return RemoveFileExtension(filename);
  }

  // Return an empty stem.
  return "";
}

std::string TextureUtils::GetOrGenerateTargetStem(const Texture &texture,
                                                  int index,
                                                  const std::string &suffix) {
  // Return target stem from |texture| if there is one.
  const std::string name = GetTargetStem(texture);
  if (!name.empty()) {
    return name;
  }

  // Return target stem generated from |index| and |suffix|.
  return "Texture" + std::to_string(index) + suffix;
}

ImageFormat TextureUtils::GetTargetFormat(const Texture &texture) {
#ifdef DRACO_SIMPLIFIER_SUPPORTED
  // Return format from |texture| compression settings unless it is empty.
  const ImageFormat format =
      texture.GetCompressionOptions().target_image_format;
  if (format != ImageFormat::NONE) {
    return format;
  }
#endif  // DRACO_SIMPLIFIER_SUPPORTED

  // Return format based on source image mime type.
  return GetSourceFormat(texture);
}

std::string TextureUtils::GetTargetExtension(const Texture &texture) {
  return GetExtension(GetTargetFormat(texture));
}

ImageFormat TextureUtils::GetSourceFormat(const Texture &texture) {
  // Try to get the extension based on source image mime type.
  std::string extension =
      LowercaseMimeTypeExtension(texture.source_image().mime_type());
  if (extension.empty() && !texture.source_image().filename().empty()) {
    // Try to get the extension from the source image filename.
    extension = LowercaseFileExtension(texture.source_image().filename());
  }
  if (extension.empty()) {
    // Default to png.
    extension = "png";
  }
  return GetFormat(extension);
}

ImageFormat TextureUtils::GetFormat(const std::string &extension) {
  if (extension == "png") {
    return ImageFormat::PNG;
  } else if (extension == "jpg" || extension == "jpeg") {
    return ImageFormat::JPEG;
  } else if (extension == "basis" || extension == "ktx2") {
    return ImageFormat::BASIS;
  } else if (extension == "webp") {
    return ImageFormat::WEBP;
  }
  return ImageFormat::NONE;
}

std::string TextureUtils::GetExtension(ImageFormat format) {
  switch (format) {
    case ImageFormat::PNG:
      return "png";
    case ImageFormat::JPEG:
      return "jpg";
    case ImageFormat::BASIS:
      return "ktx2";
    case ImageFormat::WEBP:
      return "webp";
    case ImageFormat::NONE:
    default:
      return "";
  }
}

int TextureUtils::ComputeRequiredNumChannels(
    const Texture &texture, const MaterialLibrary &material_library) {
#ifdef DRACO_SIMPLIFIER_SUPPORTED
  const auto occlusion_textures =
      FindTextures(TextureMap::AMBIENT_OCCLUSION, &material_library);
  if (std::find(occlusion_textures.begin(), occlusion_textures.end(),
                &texture) == occlusion_textures.end()) {
    // Not an occlusion texture.
    return IsTextureOpaque(texture) ? 3 : 4;
  }
#endif  // DRACO_SIMPLIFIER_SUPPORTED

  // TODO(vytyaz): Consider a case where |texture| is not only used in OMR but
  // also in other texture map types.
  const auto mr_textures = TextureUtils::FindTextures(
      TextureMap::METALLIC_ROUGHNESS, &material_library);
  if (std::find(mr_textures.begin(), mr_textures.end(), &texture) ==
      mr_textures.end()) {
    // Occlusion-only texture.
    return 1;
  }
  // Occlusion-metallic-roughness texture.
  return 3;
}

std::vector<const Texture *> TextureUtils::FindTextures(
    const TextureMap::Type texture_type,
    const MaterialLibrary *material_library) {
  // Find textures with no duplicates.
  std::unordered_set<const Texture *> textures;
  for (int i = 0; i < material_library->NumMaterials(); ++i) {
    const TextureMap *const texture_map =
        material_library->GetMaterial(i)->GetTextureMapByType(texture_type);
    if (texture_map != nullptr && texture_map->texture() != nullptr) {
      textures.insert(texture_map->texture());
    }
  }

  // Return the textures as a vector.
  std::vector<const Texture *> result;
  result.insert(result.end(), textures.begin(), textures.end());
  return result;
}

}  // namespace draco

#endif  // DRACO_TRANSCODER_SUPPORTED
