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
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "draco/core/draco_test_utils.h"
#include "draco/io/file_utils.h"
#ifdef DRACO_SIMPLIFIER_SUPPORTED
#include "draco/io/texture_image_encoder.h"
#include "draco/texture/texture_utils.h"
#endif  // DRACO_SIMPLIFIER_SUPPORTED

namespace {

#ifdef DRACO_SIMPLIFIER_SUPPORTED
TEST(TextureIoTest, TestTextureIO) {
  // A simple test that verifies that the textures are loaded and saved using
  // the texture_io.h API.
  const std::string file_name = draco::GetTestFileFullPath("test.png");
  DRACO_ASSIGN_OR_ASSERT(std::unique_ptr<draco::Texture> texture,
                         draco::ReadTextureFromFile(file_name));
  ASSERT_NE(texture, nullptr);

  // Expected dimensions of the texture are 256x256
  ASSERT_EQ(texture->width(), 256);
  ASSERT_EQ(texture->height(), 256);

  const std::string out_file_name = draco::GetTestTempFileFullPath("out.png");
  DRACO_ASSERT_OK(draco::WriteTextureToFile(out_file_name, *texture));
}

TEST(TextureIoTest, TestBadTextureIO) {
  // A simple test that verifies that we cannot load file that does not exist.
  const std::string file_name = draco::GetTestFileFullPath("test_bad.png");
  const auto maybe_texture = draco::ReadTextureFromFile(file_name);
  ASSERT_FALSE(maybe_texture.status().ok());
}

TEST(TextureIoTest, TestTransparentTexture) {
  // Verify that we can load and save a texture with transparent pixels.
  const std::string file_name =
      draco::GetTestFileFullPath("test_transparent.png");
  std::unique_ptr<draco::Texture> texture =
      draco::ReadTextureFromFile(file_name).value();
  ASSERT_NE(texture, nullptr);

  // First pixel should be fully transparent.
  ASSERT_EQ(texture->GetPixelUnsafe(0, 0).a, 0);

  // Save the texture and load it again and ensure the transparency has been
  // preserved.
  const std::string out_file_name = draco::GetTestTempFileFullPath("out.png");
  DRACO_ASSERT_OK(draco::WriteTextureToFile(out_file_name, *texture));
  std::unique_ptr<draco::Texture> texture_new =
      draco::ReadTextureFromFile(out_file_name).value();
  ASSERT_NE(texture_new, nullptr);
  ASSERT_EQ(texture->GetPixelUnsafe(0, 0).a,
            texture_new->GetPixelUnsafe(0, 0).a);
}

TEST(TextureIoTest, TestSaveToRGB) {
  // Tries to save a RGBA texture to an RGB format and read it back.
  const std::string file_name =
      draco::GetTestFileFullPath("test_transparent.png");
  std::unique_ptr<draco::Texture> texture =
      draco::ReadTextureFromFile(file_name).value();
  ASSERT_NE(texture, nullptr);
  const std::string out_file_name = draco::GetTestTempFileFullPath("out.png");
  texture->GetMutableCompressionOptions().image_compression_speed =
      draco::ImageCompressionSpeed::NORMAL;
  DRACO_ASSERT_OK(draco::WriteTextureToFile(out_file_name, *texture, 3));
  std::unique_ptr<draco::Texture> texture_new =
      draco::ReadTextureFromFile(out_file_name).value();
  ASSERT_NE(texture_new, nullptr);

  // Ensure all pixels are the same except of their alpha channel.
  for (int y = 0; y < texture->height(); ++y) {
    for (int x = 0; x < texture->width(); ++x) {
      const draco::RGBA orig_pixel = texture->GetPixelUnsafe(x, y);
      const draco::RGBA new_pixel = texture_new->GetPixelUnsafe(x, y);
      ASSERT_EQ(new_pixel.r, orig_pixel.r);
      ASSERT_EQ(new_pixel.g, orig_pixel.g);
      ASSERT_EQ(new_pixel.b, orig_pixel.b);
      ASSERT_EQ(new_pixel.a, 255);
    }
  }
}

TEST(TextureIoTest, TestExtensionMismatchFails) {
  // Tries to save texture loaded from PNG as JPEG without changing image type
  // in texture compression settings and checks that operation fails.
  const std::string file_name = draco::GetTestFileFullPath("test.png");
  std::unique_ptr<draco::Texture> texture =
      draco::ReadTextureFromFile(file_name).value();
  ASSERT_NE(texture, nullptr);
  const std::string out_file_name = draco::GetTestTempFileFullPath("out.jpg");
  texture->GetMutableCompressionOptions().image_compression_speed =
      draco::ImageCompressionSpeed::NORMAL;
  const draco::Status status =
      draco::WriteTextureToFile(out_file_name, *texture, 3);
  ASSERT_FALSE(status.ok());
  ASSERT_EQ(status.error_msg_string(),
            "Inconsistent image file extension and texture settings.");
}

TEST(TextureIoTest, TestSaveToJpeg) {
  // Tries to save a RGB texture to a Jpg file and read it back.
  const std::string file_name = draco::GetTestFileFullPath("test.png");
  std::unique_ptr<draco::Texture> texture =
      draco::ReadTextureFromFile(file_name).value();
  ASSERT_NE(texture, nullptr);
  const std::string out_file_name = draco::GetTestTempFileFullPath("out.jpg");
  texture->GetMutableCompressionOptions().image_compression_speed =
      draco::ImageCompressionSpeed::NORMAL;
  texture->GetMutableCompressionOptions().target_image_format =
      draco::ImageFormat::JPEG;
  DRACO_ASSERT_OK(draco::WriteTextureToFile(out_file_name, *texture, 3));
  std::unique_ptr<draco::Texture> texture_new =
      draco::ReadTextureFromFile(out_file_name).value();
  ASSERT_NE(texture_new, nullptr);

  // Expected dimensions of the texture are 256x256
  ASSERT_EQ(texture->width(), 256);
  ASSERT_EQ(texture->height(), 256);
}

TEST(TextureIoTest, TestCompressionSpeed) {
  // Writes three png files and checks that size of the files match expections
  // of SLOW < NORMAL < FAST.
  const std::string file_name = draco::GetTestFileFullPath("test.png");
  std::unique_ptr<draco::Texture> texture =
      draco::ReadTextureFromFile(file_name).value();
  ASSERT_NE(texture, nullptr);

  const std::string slow_file_name =
      draco::GetTestTempFileFullPath("out_slow.png");
  texture->GetMutableCompressionOptions().image_compression_speed =
      draco::ImageCompressionSpeed::SLOW;
  DRACO_ASSERT_OK(draco::WriteTextureToFile(slow_file_name, *texture, 3));
  std::unique_ptr<draco::Texture> texture_slow =
      draco::ReadTextureFromFile(slow_file_name).value();
  ASSERT_NE(texture_slow, nullptr);

  const std::string norm_file_name =
      draco::GetTestTempFileFullPath("out_norm.png");
  texture->GetMutableCompressionOptions().image_compression_speed =
      draco::ImageCompressionSpeed::NORMAL;
  DRACO_ASSERT_OK(draco::WriteTextureToFile(norm_file_name, *texture, 3));
  std::unique_ptr<draco::Texture> texture_norm =
      draco::ReadTextureFromFile(norm_file_name).value();
  ASSERT_NE(texture_norm, nullptr);

  const std::string fast_file_name =
      draco::GetTestTempFileFullPath("out_fast.png");
  texture->GetMutableCompressionOptions().image_compression_speed =
      draco::ImageCompressionSpeed::FAST;
  DRACO_ASSERT_OK(draco::WriteTextureToFile(fast_file_name, *texture, 3));
  std::unique_ptr<draco::Texture> texture_fast =
      draco::ReadTextureFromFile(fast_file_name).value();
  ASSERT_NE(texture_fast, nullptr);

  ASSERT_LT(draco::GetFileSize(slow_file_name),
            draco::GetFileSize(norm_file_name));
  ASSERT_LE(draco::GetFileSize(norm_file_name),
            draco::GetFileSize(fast_file_name));
}

TEST(TextureIoTest, TestGrayscaleIO) {
  // Tests loading and saving of grayscale images
  const std::string file_name_png =
      draco::GetTestFileFullPath("test_grayscale.png");
  std::unique_ptr<draco::Texture> texture =
      draco::ReadTextureFromFile(file_name_png).value();
  ASSERT_NE(texture, nullptr);

  const std::string out_file_name_png =
      draco::GetTestTempFileFullPath("out_grayscale.png");
  texture->GetMutableCompressionOptions().image_compression_speed =
      draco::ImageCompressionSpeed::NORMAL;
  DRACO_ASSERT_OK(draco::WriteTextureToFile(out_file_name_png, *texture, 1));

  const std::string file_name_jpg =
      draco::GetTestFileFullPath("test_grayscale.jpg");
  texture = draco::ReadTextureFromFile(file_name_jpg).value();
  ASSERT_NE(texture, nullptr);

  const std::string out_file_name_jpg =
      draco::GetTestTempFileFullPath("out_grayscale.jpg");
  texture->GetMutableCompressionOptions().image_compression_speed =
      draco::ImageCompressionSpeed::NORMAL;
  DRACO_ASSERT_OK(draco::WriteTextureToFile(out_file_name_jpg, *texture, 1));
}

TEST(TextureIoTest, TestRgbaToGrayscale) {
  // Tests that the R channel of RGBA image is used when saving as grayscale.
  constexpr uint8_t kOpaqueAlpha = 0xFF;

  // Read RGBA texture from file.
  DRACO_ASSIGN_OR_ASSERT(
      std::unique_ptr<draco::Texture> texture,
      draco::ReadTextureFromFile(draco::GetTestFileFullPath("test.png")));

  // Check an RGBA texture pixel value.
  constexpr uint8_t kExpectedRedValue = 146;
  ASSERT_EQ(texture->GetPixelUnsafe(50, 50).r, kExpectedRedValue);
  ASSERT_EQ(texture->GetPixelUnsafe(50, 50).g, 203);
  ASSERT_EQ(texture->GetPixelUnsafe(50, 50).b, 229);
  ASSERT_EQ(texture->GetPixelUnsafe(50, 50).a, kOpaqueAlpha);

  // Write the RGBA texture to file as grayscale.
  DRACO_ASSERT_OK(draco::WriteTextureToFile(
      draco::GetTestTempFileFullPath("out_grayscale.png"), *texture, 1));

  // Read the grayscale texture from file.
  DRACO_ASSIGN_OR_ASSERT(
      std::unique_ptr<draco::Texture> grayscale,
      draco::ReadTextureFromFile(
          draco::GetTestTempFileFullPath("out_grayscale.png")));

  // Check the grayscale texture pixel value. Draco grayscale preserves
  // only the R channel when working with grayscale, but image decoders output
  // monochrome to RGBA with the R, G, and B channels equal and A set to opaque.
  const draco::RGBA gray_pixel = grayscale->GetPixelUnsafe(50, 50);
  ASSERT_EQ(gray_pixel.r, kExpectedRedValue);
  ASSERT_EQ(gray_pixel.g, kExpectedRedValue);
  ASSERT_EQ(gray_pixel.r, kExpectedRedValue);
  ASSERT_EQ(gray_pixel.a, kOpaqueAlpha);
}

TEST(TextureIoTest, TestTextureNoPassThruPng) {
  // A simple test that writes the compressed png data out, as that is smaller
  // than the source png data.
  const std::string file_name = draco::GetTestFileFullPath("test.png");
  std::unique_ptr<draco::Texture> texture =
      draco::ReadTextureFromFile(file_name).value();
  ASSERT_NE(texture, nullptr);

  const std::string out_file_name = draco::GetTestTempFileFullPath("out.png");
  texture->GetMutableCompressionOptions().image_compression_speed =
      draco::ImageCompressionSpeed::NORMAL;
  DRACO_ASSERT_OK(draco::WriteTextureToFile(out_file_name, *texture, 3));
  ASSERT_GE(draco::GetFileSize(file_name), draco::GetFileSize(out_file_name));
}

TEST(TextureIoTest, TestTexturePassThruPng) {
  // A simple test that writes the source png data out, as that is smaller
  // than the compressed png data.
  const std::string file_name = draco::GetTestFileFullPath("test.png");
  std::unique_ptr<draco::Texture> texture =
      draco::ReadTextureFromFile(file_name).value();
  ASSERT_NE(texture, nullptr);

  const std::string out_file_name = draco::GetTestTempFileFullPath("out.png");
  texture->GetMutableCompressionOptions().image_compression_speed =
      draco::ImageCompressionSpeed::FAST;
  DRACO_ASSERT_OK(draco::WriteTextureToFile(out_file_name, *texture, 3));
  ASSERT_EQ(draco::GetFileSize(file_name), draco::GetFileSize(out_file_name));
}

TEST(TextureIoTest, TestTextureNoPassThruJpg) {
  // A simple test that writes the compressed jpg data out, as that is smaller
  // than the source data.
  const std::string fast_file_name = draco::GetTestFileFullPath("fast.jpg");
  std::unique_ptr<draco::Texture> texture =
      draco::ReadTextureFromFile(fast_file_name).value();
  ASSERT_NE(texture, nullptr);
  const std::string normal_file_name =
      draco::GetTestTempFileFullPath("normal.jpg");
  texture->GetMutableCompressionOptions().image_compression_speed =
      draco::ImageCompressionSpeed::NORMAL;
  DRACO_ASSERT_OK(draco::WriteTextureToFile(normal_file_name, *texture, 3));
  ASSERT_GT(draco::GetFileSize(fast_file_name),
            draco::GetFileSize(normal_file_name));
}

TEST(TextureIoTest, TestTexturePassThruJpeg) {
  // A simple test that writes source jpg data out, as that is smaller
  // than the compressed jpg data.
  const std::string normal_file_name = draco::GetTestFileFullPath("normal.jpg");
  std::unique_ptr<draco::Texture> texture =
      draco::ReadTextureFromFile(normal_file_name).value();
  ASSERT_NE(texture, nullptr);
  const std::string fast_file_name = draco::GetTestTempFileFullPath("fast.jpg");
  texture->GetMutableCompressionOptions().image_compression_speed =
      draco::ImageCompressionSpeed::FAST;
  DRACO_ASSERT_OK(draco::WriteTextureToFile(fast_file_name, *texture, 3));
  ASSERT_EQ(draco::GetFileSize(normal_file_name),
            draco::GetFileSize(fast_file_name));
}

TEST(TextureIoTest, TestTextureToBuffer) {
  const std::string file_name = draco::GetTestFileFullPath("test.png");
  std::unique_ptr<draco::Texture> texture =
      draco::ReadTextureFromFile(file_name).value();
  ASSERT_NE(texture, nullptr);

  const std::string out_file_name = draco::GetTestTempFileFullPath("out.png");
  texture->GetMutableCompressionOptions().image_compression_speed =
      draco::ImageCompressionSpeed::NORMAL;
  DRACO_ASSERT_OK(draco::WriteTextureToFile(out_file_name, *texture, 3));
  std::vector<uint8_t> buffer;
  DRACO_ASSERT_OK(WriteTextureToBuffer(*texture, 3, &buffer));
  ASSERT_EQ(buffer.size(), draco::GetFileSize(out_file_name));
}

TEST(TextureIoTest, TestTextureToBufferWithEncodedImages) {
  // Read texture from file.
  const std::string file_name = draco::GetTestFileFullPath("test.png");
  std::unique_ptr<draco::Texture> texture =
      draco::ReadTextureFromFile(file_name).value();
  ASSERT_NE(texture, nullptr);

  // Check that writing texture to buffer succeeds.
  std::vector<uint8_t> buffer;
  DRACO_ASSERT_OK(WriteTextureToBuffer(*texture, 3, &buffer));
  const int buffer_size = buffer.size();

  // Add encoded image to texture and check that writing to buffer succeeds.
  DRACO_ASSIGN_OR_ASSERT(const uint64_t texture_hash,
                         draco::TextureUtils::HashTexture(*texture));
  const draco::TextureImageEncoder::Input input = {texture.get(), texture_hash,
                                                   3};
  DRACO_ASSIGN_OR_ASSERT(auto image, draco::TextureImageEncoder::Encode(input));
  texture->GetMutableEncodedImages().push_back(std::move(image));
  buffer.clear();
  DRACO_ASSERT_OK(WriteTextureToBuffer(*texture, 3, &buffer));
  ASSERT_EQ(buffer.size(), buffer_size);

  // Modify texture image, invalidating the encoded image, and check that
  // writing to buffer fails.
  const draco::RGBA red = draco::RGBA(255, 0, 0, 255);
  texture->SetPixel(0, 0, red);
  buffer.clear();
  ASSERT_FALSE(WriteTextureToBuffer(*texture, 3, &buffer).ok());

  // Remove encoded image from texture and check that writing succeeds.
  texture->GetMutableEncodedImages().clear();
  DRACO_ASSERT_OK(WriteTextureToBuffer(*texture, 3, &buffer));
  ASSERT_NE(buffer.size(), 0);
}

TEST(TextureIoTest, TestTextureTranscodeJpgToPng) {
  // Tests that a "jpg" texture is transcoded to a "png" texture.
  const std::string source_filename = draco::GetTestFileFullPath("normal.jpg");
  DRACO_ASSIGN_OR_ASSERT(std::unique_ptr<draco::Texture> texture,
                         draco::ReadTextureFromFile(source_filename));
  const std::string target_filename =
      draco::GetTestTempFileFullPath("normal.png");
  draco::ImageCompressionOptions options;
  options.target_image_format = draco::ImageFormat::PNG;
  texture->SetCompressionOptions(options);

  DRACO_ASSERT_OK(draco::WriteTextureToFile(target_filename, *texture, 3));

  // We expect the source "jpg" to be smaller than the target "png".
  ASSERT_LT(draco::GetFileSize(source_filename),
            draco::GetFileSize(target_filename));

  // Ensure that the target is indeed "png".
  DRACO_ASSIGN_OR_ASSERT(std::unique_ptr<draco::Texture> transcoded_texture,
                         draco::ReadTextureFromFile(target_filename));
  ASSERT_EQ(transcoded_texture->source_image().mime_type(), "image/png");
}

TEST(TextureIoTest, TestTextureCopyJpeg) {
  const std::string source_filename = draco::GetTestFileFullPath("fast.jpg");
  DRACO_ASSIGN_OR_ASSERT(std::unique_ptr<draco::Texture> texture,
                         draco::ReadTextureFromFile(source_filename));
  const std::string copy_filename = draco::GetTestTempFileFullPath("copy.jpg");
  texture->GetMutableCompressionOptions().image_compression_speed =
      draco::ImageCompressionSpeed::COPY;
  DRACO_ASSERT_OK(draco::WriteTextureToFile(copy_filename, *texture, 3));
  ASSERT_EQ(draco::GetFileSize(source_filename),
            draco::GetFileSize(copy_filename));
}

TEST(TextureIoTest, TestTextureCopyPng) {
  const std::string source_filename = draco::GetTestFileFullPath("test.png");
  DRACO_ASSIGN_OR_ASSERT(std::unique_ptr<draco::Texture> texture,
                         draco::ReadTextureFromFile(source_filename));
  const std::string copy_filename = draco::GetTestTempFileFullPath("copy.png");
  texture->GetMutableCompressionOptions().image_compression_speed =
      draco::ImageCompressionSpeed::COPY;
  DRACO_ASSERT_OK(draco::WriteTextureToFile(copy_filename, *texture, 3));
  ASSERT_EQ(draco::GetFileSize(source_filename),
            draco::GetFileSize(copy_filename));
}

TEST(TextureIoTest, TestTextureCopyFail) {
  const std::string source_filename = draco::GetTestFileFullPath("test.png");
  std::unique_ptr<draco::Texture> texture =
      draco::ReadTextureFromFile(source_filename).value();
  ASSERT_NE(texture, nullptr);
  const draco::RGBA rgba = draco::RGBA(45, 90, 180, 210);
  texture->SetPixelUnsafe(1, 1, rgba);

  const std::string copy_filename = draco::GetTestTempFileFullPath("copy.png");
  texture->GetMutableCompressionOptions().image_compression_speed =
      draco::ImageCompressionSpeed::COPY;
  ASSERT_FALSE(draco::WriteTextureToFile(copy_filename, *texture, 3).ok());
}

// Tests that LOSSLESS and LOSSLESS_SLOW mode for jpeg files will just copy the
// image.
TEST(TextureIoTest, TestLosslessJpeg) {
  const std::string source_filename = draco::GetTestFileFullPath("fast.jpg");
  DRACO_ASSIGN_OR_ASSERT(std::unique_ptr<draco::Texture> texture,
                         draco::ReadTextureFromFile(source_filename));
  const std::string copy_filename = draco::GetTestTempFileFullPath("copy.jpg");
  texture->GetMutableCompressionOptions().image_compression_speed =
      draco::ImageCompressionSpeed::LOSSLESS;
  DRACO_ASSERT_OK(draco::WriteTextureToFile(copy_filename, *texture, 3));
  ASSERT_EQ(draco::GetFileSize(source_filename),
            draco::GetFileSize(copy_filename));

  const std::string copy2_filename =
      draco::GetTestTempFileFullPath("copy2.jpg");
  texture->GetMutableCompressionOptions().image_compression_speed =
      draco::ImageCompressionSpeed::LOSSLESS_SLOW;
  DRACO_ASSERT_OK(draco::WriteTextureToFile(copy2_filename, *texture, 3));
  ASSERT_EQ(draco::GetFileSize(source_filename),
            draco::GetFileSize(copy2_filename));
}

// Tests that LOSSLESS mode for png files will write out the new the image, if
// the new image is smaller than the source image. Tests that LOSSLESS_SLOW mode
// creates images smalller than LOSSLESS mode.
TEST(TextureIoTest, TestLosslessPng) {
  const std::string file_name = draco::GetTestFileFullPath("test.png");
  std::unique_ptr<draco::Texture> texture =
      draco::ReadTextureFromFile(file_name).value();
  ASSERT_NE(texture, nullptr);

  const std::string out_file_name = draco::GetTestTempFileFullPath("out.png");
  texture->GetMutableCompressionOptions().image_compression_speed =
      draco::ImageCompressionSpeed::LOSSLESS;
  DRACO_ASSERT_OK(draco::WriteTextureToFile(out_file_name, *texture, 3));
  ASSERT_GE(draco::GetFileSize(file_name), draco::GetFileSize(out_file_name));

  const std::string lossless_slow_file_name =
      draco::GetTestTempFileFullPath("lossless_slow.png");
  texture->GetMutableCompressionOptions().image_compression_speed =
      draco::ImageCompressionSpeed::LOSSLESS_SLOW;
  DRACO_ASSERT_OK(
      draco::WriteTextureToFile(lossless_slow_file_name, *texture, 3));
  ASSERT_GT(draco::GetFileSize(out_file_name),
            draco::GetFileSize(lossless_slow_file_name));
}

TEST(TextureIoTest, TestWriteToBasis) {
  // Test encoding to KTX2 file format.
  const std::string file_name = draco::GetTestFileFullPath("test.png");
  std::unique_ptr<draco::Texture> texture =
      draco::ReadTextureFromFile(file_name).value();
  ASSERT_NE(texture, nullptr);

  // Write image with no mipmaps.
  const std::string one_file_name = draco::GetTestTempFileFullPath("out.ktx2");
  texture->GetMutableCompressionOptions().generate_mipmaps = false;
  texture->GetMutableCompressionOptions().image_compression_speed =
      draco::ImageCompressionSpeed::NORMAL;
  texture->GetMutableCompressionOptions().target_image_format =
      draco::ImageFormat::BASIS;
  // Check that compression reduces the file size.
  DRACO_ASSERT_OK(draco::WriteTextureToFile(one_file_name, *texture, 3));
  ASSERT_GT(draco::GetFileSize(file_name), draco::GetFileSize(one_file_name));

  // Write image with mipmaps.
  const std::string mip_file_name = draco::GetTestTempFileFullPath("mip.ktx2");
  texture->GetMutableCompressionOptions().generate_mipmaps = true;
  DRACO_ASSERT_OK(draco::WriteTextureToFile(mip_file_name, *texture, 3));
  // Check that compression with mipmaps increases the file size compared to
  // compression without mipmaps, but not by too much.
  ASSERT_GT(draco::GetFileSize(mip_file_name),
            draco::GetFileSize(one_file_name));
  ASSERT_LT(draco::GetFileSize(mip_file_name),
            2 * draco::GetFileSize(one_file_name));
}

TEST(TextureIoTest, TestJpegQuality) {
  // Test that JPEG quality has expected effect on file size.
  const std::string test_file_name = draco::GetTestFileFullPath("test.png");
  std::unique_ptr<draco::Texture> texture =
      draco::ReadTextureFromFile(test_file_name).value();
  ASSERT_NE(texture, nullptr);

  // Get texture image file sizes with various JPEG qualities.
  texture->GetMutableCompressionOptions().target_image_format =
      draco::ImageFormat::JPEG;
  const std::string temp_file_name = draco::GetTestTempFileFullPath("temp.jpg");
  std::unordered_map<std::string, int> file_sizes;

  texture->GetMutableCompressionOptions().image_compression_speed =
      draco::ImageCompressionSpeed::SLOW;
  DRACO_ASSERT_OK(draco::WriteTextureToFile(temp_file_name, *texture, 3));
  file_sizes["default slow"] = draco::GetFileSize(temp_file_name);

  texture->GetMutableCompressionOptions().image_compression_speed =
      draco::ImageCompressionSpeed::FAST;
  DRACO_ASSERT_OK(draco::WriteTextureToFile(temp_file_name, *texture, 3));
  file_sizes["default fast"] = draco::GetFileSize(temp_file_name);

  texture->GetMutableCompressionOptions().image_compression_speed =
      draco::ImageCompressionSpeed::NORMAL;
  DRACO_ASSERT_OK(draco::WriteTextureToFile(temp_file_name, *texture, 3));
  file_sizes["default norm"] = draco::GetFileSize(temp_file_name);

  texture->GetMutableCompressionOptions().jpeg_quality = 100;
  DRACO_ASSERT_OK(draco::WriteTextureToFile(temp_file_name, *texture, 3));
  file_sizes["100 norm"] = draco::GetFileSize(temp_file_name);

  texture->GetMutableCompressionOptions().jpeg_quality = 90;
  DRACO_ASSERT_OK(draco::WriteTextureToFile(temp_file_name, *texture, 3));
  file_sizes["90 norm"] = draco::GetFileSize(temp_file_name);

  texture->GetMutableCompressionOptions().jpeg_quality = 80;
  DRACO_ASSERT_OK(draco::WriteTextureToFile(temp_file_name, *texture, 3));
  file_sizes["80 norm"] = draco::GetFileSize(temp_file_name);

  texture->GetMutableCompressionOptions().jpeg_quality = 60;
  texture->GetMutableCompressionOptions().image_compression_speed =
      draco::ImageCompressionSpeed::SLOW;
  DRACO_ASSERT_OK(draco::WriteTextureToFile(temp_file_name, *texture, 3));
  file_sizes["60 slow"] = draco::GetFileSize(temp_file_name);

  texture->GetMutableCompressionOptions().jpeg_quality = 60;
  texture->GetMutableCompressionOptions().image_compression_speed =
      draco::ImageCompressionSpeed::FAST;
  DRACO_ASSERT_OK(draco::WriteTextureToFile(temp_file_name, *texture, 3));
  file_sizes["60 fast"] = draco::GetFileSize(temp_file_name);

  texture->GetMutableCompressionOptions().jpeg_quality = 60;
  texture->GetMutableCompressionOptions().image_compression_speed =
      draco::ImageCompressionSpeed::NORMAL;
  DRACO_ASSERT_OK(draco::WriteTextureToFile(temp_file_name, *texture, 3));
  file_sizes["60 norm"] = draco::GetFileSize(temp_file_name);

  texture->GetMutableCompressionOptions().jpeg_quality = 40;
  DRACO_ASSERT_OK(draco::WriteTextureToFile(temp_file_name, *texture, 3));
  file_sizes["40 norm"] = draco::GetFileSize(temp_file_name);

  texture->GetMutableCompressionOptions().jpeg_quality = 20;
  DRACO_ASSERT_OK(draco::WriteTextureToFile(temp_file_name, *texture, 3));
  file_sizes["20 norm"] = draco::GetFileSize(temp_file_name);

  texture->GetMutableCompressionOptions().jpeg_quality = 0;
  DRACO_ASSERT_OK(draco::WriteTextureToFile(temp_file_name, *texture, 3));
  file_sizes["0 norm"] = draco::GetFileSize(temp_file_name);

  // Check that default JPEG quality is determined by compression speed.
  ASSERT_LT(file_sizes["default slow"], file_sizes["90 norm"]);
  ASSERT_EQ(file_sizes["default norm"], file_sizes["90 norm"]);
  ASSERT_EQ(file_sizes["default fast"], file_sizes["100 norm"]);

  // Check that compression speed is irrelevant if JPEG quality is set.
  ASSERT_EQ(file_sizes["60 slow"], file_sizes["60 norm"]);
  ASSERT_EQ(file_sizes["60 fast"], file_sizes["60 norm"]);

  // Check that compression improves as JPEG quality is reduced.
  ASSERT_LT(file_sizes["90 norm"], file_sizes["100 norm"]);
  ASSERT_LT(file_sizes["80 norm"], file_sizes["90 norm"]);
  ASSERT_LT(file_sizes["60 norm"], file_sizes["80 norm"]);
  ASSERT_LT(file_sizes["40 norm"], file_sizes["60 norm"]);
  ASSERT_LT(file_sizes["20 norm"], file_sizes["40 norm"]);
  ASSERT_LT(file_sizes["0 norm"], file_sizes["20 norm"]);
}

#endif  // DRACO_SIMPLIFIER_SUPPORTED

// Tests loading of textures from a buffer.
TEST(TextureIoTest, TestLoadFromBuffer) {
  const std::string file_name = draco::GetTestFileFullPath("test.png");
  std::vector<uint8_t> image_data;
  ASSERT_TRUE(draco::ReadFileToBuffer(file_name, &image_data));

  DRACO_ASSIGN_OR_ASSERT(
      std::unique_ptr<draco::Texture> texture,
      draco::ReadTextureFromBuffer(image_data.data(), image_data.size(),
                                   "image/png"));
  ASSERT_NE(texture, nullptr);

  ASSERT_EQ(texture->source_image().mime_type(), "image/png");

  // Re-encode the texture again to ensure the content hasn't changed.
  std::vector<uint8_t> encoded_buffer;
#ifdef DRACO_SIMPLIFIER_SUPPORTED
  draco::ImageCompressionOptions options;
  options.image_compression_speed = draco::ImageCompressionSpeed::COPY;
  texture->SetCompressionOptions(options);
  DRACO_ASSERT_OK(draco::WriteTextureToBuffer(*texture, 3, &encoded_buffer));
#else
  DRACO_ASSERT_OK(draco::WriteTextureToBuffer(*texture, &encoded_buffer));
#endif  // DRACO_SIMPLIFIER_SUPPORTED

  ASSERT_EQ(image_data.size(), encoded_buffer.size());
  for (int i = 0; i < encoded_buffer.size(); ++i) {
    ASSERT_EQ(image_data[i], encoded_buffer[i]);
  }
}

}  // namespace

#endif  // DRACO_TRANSCODER_SUPPORTED
