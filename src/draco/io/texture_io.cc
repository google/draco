// Copyright 2018 The Draco Authors.
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
#include "draco/io/texture_io.h"

#ifdef DRACO_TRANSCODER_SUPPORTED

#ifdef DRACO_SIMPLIFIER_SUPPORTED
#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "draco/io/basis_decoder.h"
#include "draco/io/basis_encoder.h"
#include "draco/io/file_writer_utils.h"
#include "draco/io/jpeg_decoder.h"
#include "draco/io/jpeg_encoder.h"
#include "draco/io/png_decoder.h"
#include "draco/io/png_encoder.h"
#include "draco/io/texture_image_encoder.h"
#include "draco/io/webp_decoder.h"
#include "draco/io/webp_encoder.h"
#include "draco/texture/color.h"
#include "draco/texture/texture_utils.h"
#endif  // DRACO_SIMPLIFIER_SUPPORTED

#include "draco/io/file_utils.h"

namespace draco {

namespace {

#ifdef DRACO_SIMPLIFIER_SUPPORTED
void CreateSourceImage(const std::vector<uint8_t> &encoded_data, int bit_depth,
                       const draco::Texture &texture,
                       SourceImage *out_source_image) {
  std::vector<uint8_t> &data = out_source_image->MutableEncodedData();
  data = encoded_data;

  out_source_image->set_width(texture.width());
  out_source_image->set_height(texture.height());
  out_source_image->set_bit_depth(bit_depth);

  const uint64_t hash = draco::TextureUtils::HashTexture(texture).value();
  out_source_image->set_decoded_data_hash(hash);
}

StatusOr<ImageFormat> ImageFormatFromBuffer(
    const std::vector<uint8_t> &buffer) {
  if (buffer.size() > 4) {
    // These bytes are the Start of Image (SOI) and End of Image (EOI) markers
    // in a JPEG data stream.
    const std::array<uint8_t, 2> kJpegSOIMarker = {0xFF, 0xD8};
    const std::array<uint8_t, 2> kJpegEOIMarker = {0xFF, 0xD9};

    if (!memcmp(buffer.data(), kJpegSOIMarker.data(), kJpegSOIMarker.size()) &&
        !memcmp(buffer.data() + buffer.size() - 2, kJpegEOIMarker.data(),
                kJpegEOIMarker.size())) {
      return ImageFormat::JPEG;
    }
  }

  if (buffer.size() > 2) {
    // For Binomial Basis format input the stream always begins with
    // the signature 'B' * 256 + 's', or 0x4273.
    const std::array<uint8_t, 2> kBasisSignature = {0x42, 0x73};
    if (!memcmp(buffer.data(), kBasisSignature.data(),
                kBasisSignature.size())) {
      return ImageFormat::BASIS;
    }
  }

  if (buffer.size() > 4) {
    // For Binomial Basis/KTX2 format input the stream begins with 0xab 0x4b
    // 0x54 0x58.
    const std::array<uint8_t, 4> kKtx2Signature = {0xab, 0x4b, 0x54, 0x58};
    if (!memcmp(buffer.data(), kKtx2Signature.data(), kKtx2Signature.size())) {
      return ImageFormat::BASIS;
    }
  }

  if (buffer.size() > 8) {
    // The first eight bytes of a PNG stream always contain these values:
    const std::array<uint8_t, 8> kPngSignature = {0x89, 0x50, 0x4e, 0x47,
                                                  0x0d, 0x0a, 0x1a, 0x0a};
    if (!memcmp(buffer.data(), kPngSignature.data(), kPngSignature.size())) {
      return ImageFormat::PNG;
    }
  }

  if (buffer.size() > 12) {
    // The WebP signature bytes are: RIFF 0 0 0 0 WEBP. The 0's are where WebP
    // size information is encoded in the stream, but the check here just looks
    // for RIFF and WEBP.
    const std::array<uint8_t, 4> kRIFF = {0x52, 0x49, 0x46, 0x46};
    const std::array<uint8_t, 4> kWEBP = {0x57, 0x45, 0x42, 0x50};

    if (!memcmp(buffer.data(), kRIFF.data(), kRIFF.size()) &&
        !memcmp(buffer.data() + 8, kWEBP.data(), kWEBP.size())) {
      return ImageFormat::WEBP;
    }
  }

  return Status(Status::DRACO_ERROR, "Unknown image format.");
}
#endif  // DRACO_SIMPLIFIER_SUPPORTED

StatusOr<std::unique_ptr<Texture>> CreateDracoTextureInternal(
    const std::vector<uint8_t> &image_data, SourceImage *out_source_image) {
  std::unique_ptr<Texture> draco_texture(new Texture());
#ifdef DRACO_SIMPLIFIER_SUPPORTED

  DRACO_ASSIGN_OR_RETURN(const auto format, ImageFormatFromBuffer(image_data));

  if (format == ImageFormat::BASIS) {
    DRACO_RETURN_IF_ERROR(BasisDecoder::DecodeFromBuffer(
        image_data, draco_texture->GetMutableRGBAImage()));
  } else if (format == ImageFormat::JPEG) {
    DRACO_RETURN_IF_ERROR(JpegDecoder::DecodeFromBuffer(
        image_data, draco_texture->GetMutableRGBAImage()));
  } else if (format == ImageFormat::PNG) {
    DRACO_RETURN_IF_ERROR(PngDecoder::DecodeFromBuffer(
        image_data, draco_texture->GetMutableRGBAImage()));
  } else if (format == ImageFormat::WEBP) {
    DRACO_RETURN_IF_ERROR(WebpDecoder::DecodeFromBuffer(
        image_data, draco_texture->GetMutableRGBAImage()));
  } else {
    return Status(Status::DRACO_ERROR, "Invalid input texture format.");
  }

  CreateSourceImage(image_data, /*bit_depth=*/8, *draco_texture,
                    out_source_image);
#else
  out_source_image->MutableEncodedData() = image_data;
#endif  // DRACO_SIMPLIFIER_SUPPORTED
  return std::move(draco_texture);
}

}  // namespace

StatusOr<std::unique_ptr<Texture>> ReadTextureFromFile(
    const std::string &file_name) {
  std::vector<uint8_t> image_data;
  if (!ReadFileToBuffer(file_name, &image_data)) {
    return Status(Status::IO_ERROR, "Unable to read input texture file.");
  }

  SourceImage source_image;
  DRACO_ASSIGN_OR_RETURN(auto texture,
                         CreateDracoTextureInternal(image_data, &source_image));
  source_image.set_filename(file_name);
  const std::string extension = LowercaseFileExtension(file_name);
  const std::string mime_type =
      "image/" + (extension == "jpg" ? "jpeg" : extension);
  source_image.set_mime_type(mime_type);
  texture->set_source_image(source_image);
  return texture;
}

StatusOr<std::unique_ptr<Texture>> ReadTextureFromBuffer(
    const uint8_t *buffer, size_t buffer_size, const std::string &mime_type) {
  SourceImage source_image;
  std::vector<uint8_t> image_data(buffer, buffer + buffer_size);
  DRACO_ASSIGN_OR_RETURN(auto texture,
                         CreateDracoTextureInternal(image_data, &source_image));
  source_image.set_mime_type(mime_type);
  texture->set_source_image(source_image);
  return texture;
}

#ifdef DRACO_SIMPLIFIER_SUPPORTED
Status WriteTextureToFile(const std::string &file_name,
                          const Texture &texture) {
  return WriteTextureToFile(file_name, texture, 4);
}

Status WriteTextureToFile(const std::string &file_name, const Texture &texture,
                          int num_channels) {
  if (TextureUtils::GetFormat(LowercaseFileExtension(file_name)) !=
      TextureUtils::GetTargetFormat(texture)) {
    return Status(Status::DRACO_ERROR,
                  "Inconsistent image file extension and texture settings.");
  }
  std::vector<uint8_t> buffer;
  DRACO_RETURN_IF_ERROR(WriteTextureToBuffer(texture, num_channels, &buffer));

  // Create directories if needed.
  if (!CheckAndCreatePathForFile(file_name)) {
    return Status(Status::DRACO_ERROR, "Failed to create output directories.");
  }

  if (!WriteBufferToFile(buffer.data(), buffer.size(), file_name)) {
    return Status(Status::DRACO_ERROR, "Failed to write image.");
  }

  return OkStatus();
}

// Returns true if the |texture| contains exactly the same data as its source
// image.
StatusOr<bool> IsTextureEquivalentToSource(const Texture &texture) {
  const SourceImage &si = texture.source_image();
  if (si.decoded_data_hash() == 0) {
    return false;
  }
  // Ensure the source and target texture formats are the same.
  if (TextureUtils::GetTargetFormat(texture) !=
      TextureUtils::GetSourceFormat(texture)) {
    return false;
  }
  // TODO(fgalligan): Need to check with 16bit textures.
  DRACO_ASSIGN_OR_RETURN(const uint64_t texture_hash,
                         TextureUtils::HashTexture(texture));

  // TODO(ostava): Handle hash conflicts.
  return texture_hash == si.decoded_data_hash();
}

Status WriteTextureToBuffer(const Texture &texture, int num_channels,
                            std::vector<uint8_t> *buffer) {
  // Check if the |texture| already has encoded image data that can be used
  // instead of encoding the image.
  if (!texture.GetEncodedImages().empty()) {
    // Compute hash of the uncompressed image and use it to find encoded image.
    DRACO_ASSIGN_OR_RETURN(const uint64_t texture_hash,
                           TextureUtils::HashTexture(texture));
    const TextureImageEncoder::Input input = {&texture, texture_hash,
                                              num_channels};
    const EncodedImage *const image = TextureImageEncoder::FindEncodedImage(
        texture.GetEncodedImages(), input);
    if (image != nullptr) {
      // Use an existing encoded image data.
      const auto &data = image->GetData();
      buffer->insert(buffer->end(), data.begin(), data.end());
      return OkStatus();
    }

    // Texture has encoded images but none are usable. Texture may have been
    // modified, invalidating encoded images and leading to extra memory usage.
    // TODO(vytyaz): Revisit once encoded images are passed between modules.
    return Status(Status::DRACO_ERROR, "Encoded images are unusable.");
  }

  // Check if the |texture| is equivalent to its source.
  DRACO_ASSIGN_OR_RETURN(const bool textures_are_equivalent,
                         IsTextureEquivalentToSource(texture));
  ImageCompressionSpeed updated_speed =
      texture.GetCompressionOptions().image_compression_speed;
  const ImageFormat image_format = TextureUtils::GetTargetFormat(texture);
  if (updated_speed == ImageCompressionSpeed::LOSSLESS ||
      updated_speed == ImageCompressionSpeed::LOSSLESS_SLOW) {
    // For lossless codecs set speed to NORMAL or SLOW. For lossy codecs set
    // speed to COPY.
    // TODO(b/165815819): Right now we don't use COPY mode if the texture
    // content changed, even if the codec is lossy. This should be changed as
    // part of a bigger refactoring of the image compression settings.
    if (image_format == ImageFormat::PNG || !textures_are_equivalent) {
      if (updated_speed == ImageCompressionSpeed::LOSSLESS) {
        updated_speed = ImageCompressionSpeed::NORMAL;
      } else {
        updated_speed = ImageCompressionSpeed::SLOW;
      }
    } else {
      updated_speed = ImageCompressionSpeed::COPY;
    }
  }
  if (updated_speed == ImageCompressionSpeed::COPY) {
    return CopyTextureToBuffer(texture, num_channels, buffer);
  }

  ImageCompressionOptions encode_options = texture.GetCompressionOptions();
  encode_options.image_compression_speed = updated_speed;

  ColorFormat color_format = ColorFormat::RGBA;

  if (num_channels == 1) {
    color_format = ColorFormat::Monochrome;
  } else if (num_channels == 3) {
    color_format = ColorFormat::RGB;
  }

  if (image_format == ImageFormat::BASIS) {
    DRACO_RETURN_IF_ERROR(Ktx2Encoder::EncodeToBuffer(
        color_format, texture.GetCompressionOptions(), texture.GetRGBAImage(),
        buffer));
  } else if (image_format == ImageFormat::JPEG) {
    DRACO_RETURN_IF_ERROR(JpegEncoder::EncodeToBuffer(
        color_format, encode_options, texture.GetRGBAImage(), buffer));
  } else if (image_format == ImageFormat::PNG) {
    DRACO_RETURN_IF_ERROR(PngEncoder::EncodeToBuffer(
        color_format, encode_options, texture.GetRGBAImage(), buffer));
  } else if (image_format == ImageFormat::WEBP) {
    DRACO_RETURN_IF_ERROR(WebpEncoder::EncodeToBuffer(
        color_format, encode_options, texture.GetRGBAImage(), buffer));
  }

  if (textures_are_equivalent) {
    const SourceImage &source_image = texture.source_image();

    // Check if the source data is smaller than compressed data in memory. If
    // the source data is smaller write that data out instead of the compressed
    // data in memory.
    if (!source_image.encoded_data().empty() &&
        source_image.encoded_data().size() < buffer->size()) {
      *buffer = source_image.encoded_data();
    } else if (!source_image.filename().empty() &&
               GetFileSize(source_image.filename()) < buffer->size()) {
      ReadFileToBuffer(source_image.filename(), buffer);
    }
  }

  return OkStatus();
}

Status CopyTextureToBuffer(const Texture &texture, int num_channels,
                           std::vector<uint8_t> *buffer) {
  const SourceImage &source_image = texture.source_image();
  DRACO_ASSIGN_OR_RETURN(const uint64_t hash,
                         TextureUtils::HashTexture(texture));
  const ImageFormat image_format = TextureUtils::GetTargetFormat(texture);
  if (!source_image.encoded_data().empty() ||
      !source_image.filename().empty()) {
    if (source_image.width() != texture.width() ||
        source_image.height() != texture.height()) {
      return Status(Status::DRACO_ERROR,
                    "Cannot copy texture because resolution is different.");
    }
    if (!source_image.filename().empty()) {
      if (TextureUtils::GetFormat(LowercaseFileExtension(
              source_image.filename())) != image_format) {
        return Status(Status::DRACO_ERROR,
                      "Cannot copy texture because source and target file "
                      "formats are different.");
      }
    }
    if (!source_image.mime_type().empty()) {
      if (TextureUtils::GetFormat(LowercaseMimeTypeExtension(
              source_image.mime_type())) != image_format) {
        return Status(Status::DRACO_ERROR,
                      "Cannot copy texture because source and target file "
                      "formats are different.");
      }
    }
    if (hash != source_image.decoded_data_hash()) {
      return Status(Status::DRACO_ERROR,
                    "Cannot copy texture because it has been modified.");
    }

    // Check if source image already has the encoded data.
    if (!source_image.encoded_data().empty()) {
      *buffer = source_image.encoded_data();
    } else {
      if (!ReadFileToBuffer(source_image.filename(), buffer)) {
        return Status(Status::DRACO_ERROR, "Failed to read source image");
      }
    }
  } else {
    return Status(Status::DRACO_ERROR, "No texture source data to copy.");
  }
  return OkStatus();
}
#else   // !DRACO_SIMPLIFIER_SUPPORTED
Status WriteTextureToFile(const std::string &file_name,
                          const Texture &texture) {
  std::vector<uint8_t> buffer;
  DRACO_RETURN_IF_ERROR(WriteTextureToBuffer(texture, &buffer));

  if (!WriteBufferToFile(buffer.data(), buffer.size(), file_name)) {
    return Status(Status::DRACO_ERROR, "Failed to write image.");
  }

  return OkStatus();
}

Status WriteTextureToBuffer(const Texture &texture,
                            std::vector<uint8_t> *buffer) {
  // Copy data from the encoded source image if possible, otherwise load the
  // data from the source file.
  if (!texture.source_image().encoded_data().empty()) {
    *buffer = texture.source_image().encoded_data();
  } else if (!texture.source_image().filename().empty()) {
    if (!ReadFileToBuffer(texture.source_image().filename(), buffer)) {
      return Status(Status::IO_ERROR, "Unable to read input texture file.");
    }
  } else {
    return Status(Status::DRACO_ERROR, "Invalid source data for the texture.");
  }
  return OkStatus();
}
#endif  // DRACO_SIMPLIFIER_SUPPORTED

}  // namespace draco

#endif  // DRACO_TRANSCODER_SUPPORTED
