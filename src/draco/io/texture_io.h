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
#ifndef DRACO_IO_TEXTURE_IO_H_
#define DRACO_IO_TEXTURE_IO_H_

#include "draco/draco_features.h"

#ifdef DRACO_TRANSCODER_SUPPORTED
#include <memory>

#include "draco/core/draco_types.h"
#include "draco/core/status_or.h"
#include "draco/texture/texture.h"

namespace draco {

// Reads a texture from a file. Reads PNG, JPEG and WEBP texture files.
// Returns nullptr with an error status if the decoding failed.
StatusOr<std::unique_ptr<Texture>> ReadTextureFromFile(
    const std::string &file_name);

// Same as ReadTextureFromFile() but the texture data is parsed from a |buffer|.
// |mime_type| should be set to a type of the texture encoded in |buffer|.
// Supported mime types are "image/jpeg", "image/png" and "image/webp".
// TODO(ostava): We should be able to get the mime type directly from the
// |buffer| but our image decoding library doesn't support this at this time.
StatusOr<std::unique_ptr<Texture>> ReadTextureFromBuffer(
    const uint8_t *buffer, size_t buffer_size, const std::string &mime_type);

// Writes a texture into a file. Can write PNG, JPEG, WEBP, and KTX2 (with Basis
// compression) texture files depending on the extension specified in
// |file_name| and image format specified in |texture|. Note that images with
// Basis compression can only be saved to files in KTX2 format and not to files
// with "basis" extension. Returns an error status if the writing failed.
Status WriteTextureToFile(const std::string &file_name, const Texture &texture);

#ifdef DRACO_SIMPLIFIER_SUPPORTED
// Writes a texture into a file in a specified format defined by the
// |num_channels|. The function will try to convert the data in |texture| to the
// desired output format before the texture is saved to the file. If the
// conversion fails, an error status is returned. Currently, the only allowed
// options are:
//   - |num_channels| == 3 | 4
//   - |num_channels| == 1 saves the R channel and ignores G and B channels.
Status WriteTextureToFile(const std::string &file_name, const Texture &texture,
                          int num_channels);

// Writes a |texture| into |buffer| specified by the parameters (see above
// comments). The image format is specified in |texture|. Supported image types
// are PNG, JPEG, and KTX2 (with Basis compression). Note that images with
// Basis compression can only be saved in KTX2 format.
Status WriteTextureToBuffer(const Texture &texture, int num_channels,
                            std::vector<uint8_t> *buffer);

// Copies a texture into |buffer|. Only copies the source if output
// characteristics match the source characteristics. The function will fail if
// the source image format is different from the image type in |texture|
// compression settings.
Status CopyTextureToBuffer(const Texture &texture, int num_channels,
                           std::vector<uint8_t> *buffer);
#else
// Writes a |texture| into |buffer|.
Status WriteTextureToBuffer(const Texture &texture,
                            std::vector<uint8_t> *buffer);
#endif  // DRACO_SIMPLIFIER_SUPPORTED

}  // namespace draco

#endif  // DRACO_TRANSCODER_SUPPORTED
#endif  // DRACO_IO_TEXTURE_IO_H_
