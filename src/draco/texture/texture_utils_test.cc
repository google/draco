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
#include <utility>

#include "draco/core/draco_test_utils.h"
#include "draco/io/texture_io.h"
#include "draco/texture/color_utils.h"

namespace {

#ifdef DRACO_SIMPLIFIER_SUPPORTED
// Helper struct for texture downsampling tests.
struct TestHelper {
  // Returns a |width| by |height| texture with a gradient color.
  static std::unique_ptr<draco::Texture> CreateTexture(int width, int height) {
    std::unique_ptr<draco::Texture> output =
        std::unique_ptr<draco::Texture>(new draco::Texture());
    output->Resize(width, height);
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        output->SetPixelUnsafe(
            x, y, draco::RGBA(x, y, std::max(x, y), std::min(x, y)));
      }
    }
    return output;
  }

  // Returns |input| texture downsampled to a squre texture via given
  // intermediate |resolutions| like {512, 256, 128}.
  static std::unique_ptr<draco::Texture> Downsample(
      const draco::Texture &input,
      const std::vector<std::vector<int>> &resolutions) {
    draco::Texture intermediate;
    std::unique_ptr<draco::Texture> output =
        std::unique_ptr<draco::Texture>(new draco::Texture());
    output->Copy(input);
    for (const std::vector<int> &resolution : resolutions) {
      draco::TextureUtils::ResampleTexture(
          *output, resolution[0], resolution[1],
          draco::TextureMap::WrappingMode(draco::TextureMap::CLAMP_TO_EDGE),
          &intermediate);
      output->Copy(intermediate);
    }
    return output;
  }

  // Returns |input| texture downsampled to a rectangular texture via given
  // intermediate |resolutions| like {{512, 400}, {256, 380}, {128, 360}}.
  static std::unique_ptr<draco::Texture> Downsample(
      const draco::Texture &input, const std::vector<int> &resolutions) {
    std::vector<std::vector<int>> resolution_pairs;
    resolution_pairs.reserve(resolutions.size());
    for (const int resolution : resolutions) {
      resolution_pairs.push_back({resolution, resolution});
    }
    return Downsample(input, resolution_pairs);
  }
};

TEST(TextureUtilsTest, TestOpaqueTexture) {
  // Tests whether draco::TextureUtils properly detects opaque textures.
  std::unique_ptr<draco::Texture> texture =
      draco::ReadTextureFromFile(draco::GetTestFileFullPath("test.png"))
          .value();
  ASSERT_NE(texture, nullptr);
  ASSERT_TRUE(draco::TextureUtils::IsTextureOpaque(*texture));
  ASSERT_FALSE(draco::TextureUtils::IsTextureFullyTransparent(*texture));
}

TEST(TextureUtilsTest, TestTransparentTexture) {
  // Tests whether draco::TextureUtils properly detects transparent textures.
  std::unique_ptr<draco::Texture> texture =
      draco::ReadTextureFromFile(
          draco::GetTestFileFullPath("test_transparent.png"))
          .value();
  ASSERT_NE(texture, nullptr);
  ASSERT_FALSE(draco::TextureUtils::IsTextureOpaque(*texture));
  ASSERT_FALSE(draco::TextureUtils::IsTextureFullyTransparent(*texture));
}

TEST(TextureUtilsTest, TestFullyTransparentTexture) {
  // Tests whether draco::TextureUtils properly detects fully transparent
  // textures.
  std::unique_ptr<draco::Texture> texture =
      draco::ReadTextureFromFile(
          draco::GetTestFileFullPath("fully_transparent.png"))
          .value();
  ASSERT_NE(texture, nullptr);
  ASSERT_FALSE(draco::TextureUtils::IsTextureOpaque(*texture));
  ASSERT_TRUE(draco::TextureUtils::IsTextureFullyTransparent(*texture));
}

TEST(TextureUtilsTest, TestResampleTexture) {
  // Tests whether draco::TextureUtils properly resamples an input texture.
  const auto source_texture = TestHelper::CreateTexture(256, 256);

  draco::Texture resampled_texture;
  DRACO_ASSERT_OK(draco::TextureUtils::ResampleTexture(
      *source_texture, 128, 128,
      draco::TextureMap::WrappingMode(draco::TextureMap::CLAMP_TO_EDGE),
      &resampled_texture));

  for (int y = 0; y < 128; ++y) {
    for (int x = 0; x < 128; ++x) {
      // Average pixels in a 2x2 block from the source texture.
      draco::Vector4f sampled_color;
      for (int i = 0; i < 2; ++i) {
        for (int j = 0; j < 2; ++j) {
          sampled_color =
              sampled_color +
              draco::ColorUtils::ToVector4f(draco::ColorUtils::DequantizeColor(
                  source_texture->GetPixelUnsafe(2 * x + i, 2 * y + j)));
        }
      }
      // Average the sampled values.
      sampled_color = sampled_color / 4.f;
      const draco::RGBA sampled_color_pixel = draco::ColorUtils::QuantizeColor(
          draco::ColorUtils::ToRGBAf(sampled_color));
      ASSERT_TRUE(draco::ColorUtils::AreColorsSimilar(
          sampled_color_pixel, resampled_texture.GetPixelUnsafe(x, y), 1));
    }
  }
}

TEST(TextureUtilsTest, TestResizeTexture) {
  // Tests whether draco::TextureUtils properly resizes an input texture.
  const auto source_texture = TestHelper::CreateTexture(256, 256);

  draco::Texture resized_texture_0;
  DRACO_ASSERT_OK(draco::TextureUtils::ResizeTexture(
      *source_texture, 128, 128,
      draco::TextureMap::WrappingMode(draco::TextureMap::CLAMP_TO_EDGE),
      &resized_texture_0));

  // The above resized texture should be equivalent to resampled texture since
  // we downsample only to half of the original resolution.
  draco::Texture resampled_texture;
  DRACO_ASSERT_OK(draco::TextureUtils::ResampleTexture(
      *source_texture, 128, 128,
      draco::TextureMap::WrappingMode(draco::TextureMap::CLAMP_TO_EDGE),
      &resampled_texture));

  for (int y = 0; y < 128; ++y) {
    for (int x = 0; x < 128; ++x) {
      ASSERT_TRUE(draco::ColorUtils::AreColorsSimilar(
          resized_texture_0.GetPixelUnsafe(x, y),
          resampled_texture.GetPixelUnsafe(x, y), 1));
    }
  }

  // Resize the texture to quater of the original resolution.
  draco::Texture resized_texture_1;
  DRACO_ASSERT_OK(draco::TextureUtils::ResizeTexture(
      *source_texture, 64, 64,
      draco::TextureMap::WrappingMode(draco::TextureMap::CLAMP_TO_EDGE),
      &resized_texture_1));

  for (int y = 0; y < 64; ++y) {
    for (int x = 0; x < 64; ++x) {
      // Average pixels in a 4x4 block from the source texture.
      draco::Vector4f sampled_color;
      for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
          sampled_color =
              sampled_color +
              draco::ColorUtils::ToVector4f(draco::ColorUtils::DequantizeColor(
                  source_texture->GetPixelUnsafe(4 * x + i, 4 * y + j)));
        }
      }
      // Average the sampled values.
      sampled_color = sampled_color / 16.f;
      const draco::RGBA sampled_color_pixel = draco::ColorUtils::QuantizeColor(
          draco::ColorUtils::ToRGBAf(sampled_color));
      ASSERT_TRUE(draco::ColorUtils::AreColorsSimilar(
          sampled_color_pixel, resized_texture_1.GetPixelUnsafe(x, y), 1));
    }
  }
}

TEST(TextureUtilsTest, TestResizeTextureIntermediateResolutions) {
  // Tests that correct intermediate resolutions are used when resizing texture.

  // Create textures for testing.
  const auto source_texture_200 = TestHelper::CreateTexture(200, 200);
  const auto source_texture_600_by_200 = TestHelper::CreateTexture(600, 200);
  const auto source_texture_65_by_63 = TestHelper::CreateTexture(65, 63);
  const auto source_texture_512_by_256 = TestHelper::CreateTexture(512, 256);

  // Check that downsampling result depends on intermediate resolutions.
  const auto tex_200_to_32_via_divisors =
      TestHelper::Downsample(*source_texture_200, {100, 50, 32});
  const auto tex_200_to_32_via_multiples =
      TestHelper::Downsample(*source_texture_200, {128, 64, 32});
  ASSERT_NE(
      draco::TextureUtils::HashTexture(*tex_200_to_32_via_divisors).value(),
      draco::TextureUtils::HashTexture(*tex_200_to_32_via_multiples).value());

  // Check that when the target resolution is a power of two then intermediate
  // resolutions in the ResizeTexture() method are also powers of two.
  draco::Texture resized_200_to_32;
  DRACO_ASSERT_OK(draco::TextureUtils::ResizeTexture(
      *source_texture_200, 32, 32,
      draco::TextureMap::WrappingMode(draco::TextureMap::CLAMP_TO_EDGE),
      &resized_200_to_32));
  ASSERT_EQ(draco::TextureUtils::HashTexture(resized_200_to_32).value(),
            draco::TextureUtils::HashTexture(
                *TestHelper::Downsample(*source_texture_200, {128, 64, 32}))
                .value());

  // Check that when the target resolution is not a power of two then
  // intermediate resolutions in the ResizeTexture() method are divisors of the
  // input resolution.
  draco::Texture resized_200_to_40;
  DRACO_ASSERT_OK(draco::TextureUtils::ResizeTexture(
      *source_texture_200, 40, 40,
      draco::TextureMap::WrappingMode(draco::TextureMap::CLAMP_TO_EDGE),
      &resized_200_to_40));
  ASSERT_EQ(draco::TextureUtils::HashTexture(resized_200_to_40).value(),
            draco::TextureUtils::HashTexture(
                *TestHelper::Downsample(*source_texture_200, {100, 50, 40}))
                .value());

  // Check that intermediate resolutions are correct for rectangular texture.
  draco::Texture resized_600_by_200;
  DRACO_ASSERT_OK(draco::TextureUtils::ResizeTexture(
      *source_texture_600_by_200, 20, 32,
      draco::TextureMap::WrappingMode(draco::TextureMap::CLAMP_TO_EDGE),
      &resized_600_by_200));
  ASSERT_EQ(draco::TextureUtils::HashTexture(resized_600_by_200).value(),
            draco::TextureUtils::HashTexture(
                *TestHelper::Downsample(
                    *source_texture_600_by_200,
                    {{300, 128}, {150, 64}, {75, 32}, {37, 32}, {20, 32}}))
                .value());

  // Check that intermediate resolutions are correct for texture with resolution
  // that is off-by-one from a power of two, such as 65-by-63.
  draco::Texture resized_65_by_63;
  DRACO_ASSERT_OK(draco::TextureUtils::ResizeTexture(
      *source_texture_65_by_63, 32, 30,
      draco::TextureMap::WrappingMode(draco::TextureMap::CLAMP_TO_EDGE),
      &resized_65_by_63));
  ASSERT_EQ(draco::TextureUtils::HashTexture(resized_65_by_63).value(),
            draco::TextureUtils::HashTexture(
                *TestHelper::Downsample(*source_texture_65_by_63,
                                        {{64, 31}, {32, 30}}))
                .value());

  // Check that intermediate resolutions are square for a rectangular texture
  // that is downsized to a square texture.
  draco::Texture resized_512_by_256;
  DRACO_ASSERT_OK(draco::TextureUtils::ResizeTexture(
      *source_texture_512_by_256, 128, 128,
      draco::TextureMap::WrappingMode(draco::TextureMap::CLAMP_TO_EDGE),
      &resized_512_by_256));
  ASSERT_EQ(draco::TextureUtils::HashTexture(resized_512_by_256).value(),
            draco::TextureUtils::HashTexture(
                *TestHelper::Downsample(*source_texture_512_by_256,
                                        {{256, 256}, {128, 128}}))
                .value());
}

TEST(TextureUtilsTest, TestEquivalent) {
  std::unique_ptr<draco::Texture> texture_a =
      draco::ReadTextureFromFile(draco::GetTestFileFullPath("test.png"))
          .value();
  ASSERT_NE(texture_a, nullptr);
  std::unique_ptr<draco::Texture> texture_b =
      draco::ReadTextureFromFile(draco::GetTestFileFullPath("test.png"))
          .value();
  ASSERT_NE(texture_b, nullptr);
  ASSERT_TRUE(
      draco::TextureUtils::AreTexturesEquivalent(*texture_a, *texture_b));
}

TEST(TextureUtilsTest, TestSimilar) {
  std::unique_ptr<draco::Texture> texture_a =
      draco::ReadTextureFromFile(draco::GetTestFileFullPath("test.png"))
          .value();
  ASSERT_NE(texture_a, nullptr);
  std::unique_ptr<draco::Texture> texture_b =
      draco::ReadTextureFromFile(draco::GetTestFileFullPath("test.png"))
          .value();
  ASSERT_NE(texture_b, nullptr);
  ASSERT_TRUE(
      draco::TextureUtils::AreTexturesSimilar(*texture_a, *texture_b, 0));

  // Change a pixel and check for similarity again.
  const draco::RGBA rgba_a = draco::RGBA(45, 92, 180, 210);
  texture_a->SetPixelUnsafe(1, 1, rgba_a);
  const draco::RGBA rgba_b = draco::RGBA(46, 90, 181, 211);
  texture_b->SetPixelUnsafe(1, 1, rgba_b);
  ASSERT_FALSE(
      draco::TextureUtils::AreTexturesSimilar(*texture_a, *texture_b, 0));
  ASSERT_TRUE(
      draco::TextureUtils::AreTexturesSimilar(*texture_a, *texture_b, 2));
}

TEST(TextureUtilsTest, TestNotEquivalent) {
  std::unique_ptr<draco::Texture> texture_a =
      draco::ReadTextureFromFile(draco::GetTestFileFullPath("test.png"))
          .value();
  ASSERT_NE(texture_a, nullptr);
  std::unique_ptr<draco::Texture> texture_b =
      draco::ReadTextureFromFile(
          draco::GetTestFileFullPath("test_transparent.png"))
          .value();
  ASSERT_NE(texture_b, nullptr);
  ASSERT_FALSE(
      draco::TextureUtils::AreTexturesEquivalent(*texture_a, *texture_b));
}

TEST(TextureUtilsTest, TestNotEquivalentResize) {
  std::unique_ptr<draco::Texture> texture_a =
      draco::ReadTextureFromFile(draco::GetTestFileFullPath("test.png"))
          .value();
  ASSERT_NE(texture_a, nullptr);

  draco::Texture resized_texture_a;
  DRACO_ASSERT_OK(draco::TextureUtils::ResizeTexture(
      *texture_a, 128, 128,
      draco::TextureMap::WrappingMode(draco::TextureMap::CLAMP_TO_EDGE),
      &resized_texture_a));
  ASSERT_FALSE(draco::TextureUtils::AreTexturesEquivalent(*texture_a,
                                                          resized_texture_a));

  // Check source image information is included in the resized texture.
  ASSERT_EQ(texture_a->source_image().filename(),
            resized_texture_a.source_image().filename());
  ASSERT_EQ(texture_a->source_image().mime_type(),
            resized_texture_a.source_image().mime_type());
}

TEST(TextureUtilsTest, TestHash) {
  DRACO_ASSIGN_OR_ASSERT(
      std::unique_ptr<draco::Texture> texture,
      draco::ReadTextureFromFile(draco::GetTestFileFullPath("test.png")));
  DRACO_ASSIGN_OR_ASSERT(uint64_t hash,
                         draco::TextureUtils::HashTexture(*texture));
  ASSERT_GT(hash, 0);

  const draco::RGBA rgba = draco::RGBA(45, 90, 180, 210);
  texture->SetPixelUnsafe(1, 1, rgba);
  DRACO_ASSIGN_OR_ASSERT(uint64_t new_hash,
                         draco::TextureUtils::HashTexture(*texture));
  ASSERT_GT(new_hash, 0);
  ASSERT_NE(hash, new_hash);
}

TEST(TextureUtilsTest, Test1x1Hash) {
  DRACO_ASSIGN_OR_ASSERT(
      std::unique_ptr<draco::Texture> texture,
      draco::ReadTextureFromFile(draco::GetTestFileFullPath("1x1.png")));
  DRACO_ASSIGN_OR_ASSERT(uint64_t hash,
                         draco::TextureUtils::HashTexture(*texture));
  ASSERT_EQ(hash, 18446744071387827139ULL);
}

TEST(TextureUtilsTest, TestTexturePixelComponentsIdentical) {
  // black.png is contant throughout.
  std::unique_ptr<draco::Texture> texture =
      draco::ReadTextureFromFile(draco::GetTestFileFullPath("black.png"))
          .value();
  ASSERT_NE(texture, nullptr);
  ASSERT_TRUE(draco::TextureUtils::TexturePixelComponentsEqual(
      *texture, true, true, true, true));

  // red.png is constant throughout.
  texture =
      draco::ReadTextureFromFile(draco::GetTestFileFullPath("red.png")).value();
  ASSERT_NE(texture, nullptr);
  ASSERT_TRUE(draco::TextureUtils::TexturePixelComponentsEqual(
      *texture, true, true, true, true));

  // test.png has constant alpha, but varies in the other components.
  texture = draco::ReadTextureFromFile(draco::GetTestFileFullPath("test.png"))
                .value();
  ASSERT_NE(texture, nullptr);
  ASSERT_FALSE(draco::TextureUtils::TexturePixelComponentsEqual(
      *texture, true, true, true, true));
  ASSERT_TRUE(draco::TextureUtils::TexturePixelComponentsEqual(
      *texture, false, false, false, true));

  // squares.png has constant green and alpha components. Red and blue change
  // throughout.
  texture =
      draco::ReadTextureFromFile(draco::GetTestFileFullPath("squares.png"))
          .value();
  ASSERT_NE(texture, nullptr);
  ASSERT_FALSE(draco::TextureUtils::TexturePixelComponentsEqual(
      *texture, true, true, true, true));
  ASSERT_TRUE(draco::TextureUtils::TexturePixelComponentsEqual(
      *texture, false, false, false, true));
  ASSERT_TRUE(draco::TextureUtils::TexturePixelComponentsEqual(
      *texture, false, true, false, false));
  ASSERT_TRUE(draco::TextureUtils::TexturePixelComponentsEqual(
      *texture, false, true, false, true));
}

TEST(TextureUtilsTest, TestJpegSourceImage) {
  std::unique_ptr<draco::Texture> texture_a =
      draco::ReadTextureFromFile(draco::GetTestFileFullPath("fast.jpg"))
          .value();
  ASSERT_NE(texture_a, nullptr);

  draco::Texture resized_texture_a;
  DRACO_ASSERT_OK(draco::TextureUtils::ResizeTexture(
      *texture_a, 128, 128,
      draco::TextureMap::WrappingMode(draco::TextureMap::CLAMP_TO_EDGE),
      &resized_texture_a));
  ASSERT_FALSE(draco::TextureUtils::AreTexturesEquivalent(*texture_a,
                                                          resized_texture_a));

  // Check source image information is included in the resized texture.
  ASSERT_EQ(texture_a->source_image().filename(),
            resized_texture_a.source_image().filename());
  ASSERT_EQ(texture_a->source_image().mime_type(),
            resized_texture_a.source_image().mime_type());
}
#endif  // DRACO_SIMPLIFIER_SUPPORTED

TEST(TextureUtilsTest, TestGetTargetNameForTextureLoadedFromFile) {
  // Tests that correct target stem and format are returned by texture utils for
  // texture loaded from image file (stem and format from source file).
  std::unique_ptr<draco::Texture> texture =
      draco::ReadTextureFromFile(draco::GetTestFileFullPath("fast.jpg"))
          .value();
  ASSERT_NE(texture, nullptr);
  ASSERT_EQ(draco::TextureUtils::GetTargetStem(*texture), "fast");
  ASSERT_EQ(draco::TextureUtils::GetTargetExtension(*texture), "jpg");
  ASSERT_EQ(draco::TextureUtils::GetTargetFormat(*texture),
            draco::ImageFormat::JPEG);
  ASSERT_EQ(draco::TextureUtils::GetOrGenerateTargetStem(*texture, 5, "_Color"),
            "fast");

#ifdef DRACO_SIMPLIFIER_SUPPORTED
  // Change image type in texture settings and check that format changes.
  texture->GetMutableCompressionOptions().target_image_format =
      draco::ImageFormat::BASIS;
  ASSERT_EQ(draco::TextureUtils::GetTargetFormat(*texture),
            draco::ImageFormat::BASIS);
#endif  // DRACO_SIMPLIFIER_SUPPORTED
}

TEST(TextureUtilsTest, TestGetTargetNameForNewTexture) {
  // Tests that correct target stem and format are returned by texture utils for
  // a newly created texture (empty stem and PNG image type by default).
  std::unique_ptr<draco::Texture> texture(new draco::Texture());
  ASSERT_NE(texture, nullptr);
  ASSERT_EQ(draco::TextureUtils::GetTargetStem(*texture), "");
  ASSERT_EQ(draco::TextureUtils::GetOrGenerateTargetStem(*texture, 5, "_Color"),
            "Texture5_Color");
  ASSERT_EQ(draco::TextureUtils::GetTargetExtension(*texture), "png");
  ASSERT_EQ(draco::TextureUtils::GetTargetFormat(*texture),
            draco::ImageFormat::PNG);

#ifdef DRACO_SIMPLIFIER_SUPPORTED
  // Change image type in texture settings and check that format changes.
  texture->GetMutableCompressionOptions().target_image_format =
      draco::ImageFormat::BASIS;
  ASSERT_EQ(draco::TextureUtils::GetTargetExtension(*texture), "ktx2");
  ASSERT_EQ(draco::TextureUtils::GetTargetFormat(*texture),
            draco::ImageFormat::BASIS);
#endif  // DRACO_SIMPLIFIER_SUPPORTED
}

TEST(TextureUtilsTest, TestGetSourceFormat) {
  // Tests that the source format is determined correctly for new textures and
  // for textures loaded from file.
  std::unique_ptr<draco::Texture> new_texture(new draco::Texture());
  DRACO_ASSIGN_OR_ASSERT(
      std::unique_ptr<draco::Texture> png_texture,
      draco::ReadTextureFromFile(draco::GetTestFileFullPath("test.png")));
  DRACO_ASSIGN_OR_ASSERT(
      std::unique_ptr<draco::Texture> jpg_texture,
      draco::ReadTextureFromFile(draco::GetTestFileFullPath("fast.jpg")));

  // Check source formats.
  ASSERT_EQ(draco::TextureUtils::GetSourceFormat(*new_texture),
            draco::ImageFormat::PNG);
  ASSERT_EQ(draco::TextureUtils::GetSourceFormat(*png_texture),
            draco::ImageFormat::PNG);
  ASSERT_EQ(draco::TextureUtils::GetSourceFormat(*jpg_texture),
            draco::ImageFormat::JPEG);

  // Remove the mime-type from the jpeg texture and ensure the source format is
  // still detected properly based on the filename.
  jpg_texture->source_image().set_mime_type("");
  ASSERT_EQ(draco::TextureUtils::GetSourceFormat(*jpg_texture),
            draco::ImageFormat::JPEG);
}

#ifdef DRACO_SIMPLIFIER_SUPPORTED
TEST(TextureUtilsTest, TestSetCompressionOptions) {
  // Tests that compression options are correctly set to texture library.
  draco::TextureLibrary library;
  library.PushTexture(
      draco::ReadTextureFromFile(draco::GetTestFileFullPath("test.png"))
          .value());
  library.PushTexture(
      draco::ReadTextureFromFile(draco::GetTestFileFullPath("fast.jpg"))
          .value());

  // Change compression settings for all textures in the library.
  draco::ImageCompressionOptions options;
  options.image_compression_speed = draco::ImageCompressionSpeed::FAST;
  options.target_image_format = draco::ImageFormat::BASIS;
  draco::TextureUtils::SetCompressionOptions(options, &library);

  // Check that compression settings are changed correctly.
  for (int i : {0, 1}) {
    ASSERT_EQ(
        library.GetTexture(i)->GetCompressionOptions().image_compression_speed,
        draco::ImageCompressionSpeed::FAST);
    ASSERT_EQ(
        library.GetTexture(i)->GetCompressionOptions().target_image_format,
        draco::ImageFormat::BASIS);
  }
}

TEST(TextureUtilsTest, TestSetCompressionOptionsToTextureType) {
  // Tests that compression options are correctly set to texture images of a
  // specific texture type.
  std::unique_ptr<draco::Mesh> mesh =
      draco::ReadMeshFromTestFile("SphereAllSame/sphere_texture_all.gltf");
  ASSERT_NE(mesh, nullptr);
  draco::MaterialLibrary &library = mesh->GetMaterialLibrary();
  ASSERT_EQ(library.NumMaterials(), 1);
  const draco::Material &material = *library.GetMaterial(0);
  ASSERT_EQ(material.NumTextureMaps(), 5);

  // Helper struct for checking compression settings for a texture type.
  struct Result {
    draco::TextureMap::Type type;
    draco::ImageCompressionOptions options;
  };

  // Check that the initial image compression settings have default values.
  draco::ImageCompressionOptions default_options;
  std::vector<Result> initial_results = {
      {draco::TextureMap::COLOR, default_options},
      {draco::TextureMap::EMISSIVE, default_options},
      {draco::TextureMap::METALLIC_ROUGHNESS, default_options},
      {draco::TextureMap::AMBIENT_OCCLUSION, default_options},
      {draco::TextureMap::NORMAL_TANGENT_SPACE, default_options}};

  for (const Result &result : initial_results) {
    ASSERT_NE(material.GetTextureMapByType(result.type), nullptr);
    const draco::ImageCompressionOptions &options =
        material.GetTextureMapByType(result.type)
            ->texture()
            ->GetCompressionOptions();
    ASSERT_EQ(options.image_compression_speed,
              result.options.image_compression_speed);
    ASSERT_EQ(options.target_image_format, result.options.target_image_format);
  }

  // Change compression settings for color texture.
  draco::ImageCompressionOptions color_options;
  color_options.image_compression_speed = draco::ImageCompressionSpeed::FAST;
  color_options.target_image_format = draco::ImageFormat::BASIS;
  draco::TextureUtils::SetCompressionOptions(
      color_options, draco::TextureMap::COLOR, &library);

  // Change compression settings for normal texture.
  draco::ImageCompressionOptions normal_options;
  normal_options.image_compression_speed = draco::ImageCompressionSpeed::SLOW;
  normal_options.target_image_format = draco::ImageFormat::PNG;
  draco::TextureUtils::SetCompressionOptions(
      normal_options, draco::TextureMap::NORMAL_TANGENT_SPACE, &library);

  // Check that compression settings are changed correctly.
  std::vector<Result> changed_results = {
      {draco::TextureMap::COLOR, color_options},
      {draco::TextureMap::EMISSIVE, default_options},
      {draco::TextureMap::METALLIC_ROUGHNESS, default_options},
      {draco::TextureMap::AMBIENT_OCCLUSION, default_options},
      {draco::TextureMap::NORMAL_TANGENT_SPACE, normal_options}};

  for (const Result &result : changed_results) {
    ASSERT_NE(material.GetTextureMapByType(result.type), nullptr);
    const draco::ImageCompressionOptions &options =
        material.GetTextureMapByType(result.type)
            ->texture()
            ->GetCompressionOptions();
    ASSERT_EQ(options.image_compression_speed,
              result.options.image_compression_speed);
    ASSERT_EQ(options.target_image_format, result.options.target_image_format);
  }
}

TEST(TextureUtilsTest, TestSetCompressionSpeed) {
  // Tests that compression speed is correctly set to texture library.
  draco::TextureLibrary library;
  library.PushTexture(
      draco::ReadTextureFromFile(draco::GetTestFileFullPath("test.png"))
          .value());
  library.PushTexture(
      draco::ReadTextureFromFile(draco::GetTestFileFullPath("fast.jpg"))
          .value());

  // Change compression speed for all textures in the library.
  draco::TextureUtils::SetCompressionSpeed(draco::ImageCompressionSpeed::FAST,
                                           &library);

  // Check that compression settings are changed correctly.
  for (int i : {0, 1}) {
    ASSERT_EQ(
        library.GetTexture(i)->GetCompressionOptions().image_compression_speed,
        draco::ImageCompressionSpeed::FAST);
    ASSERT_EQ(
        library.GetTexture(i)->GetCompressionOptions().target_image_format,
        draco::ImageFormat::NONE);
  }
}
#endif  // DRACO_SIMPLIFIER_SUPPORTED

TEST(TextureUtilsTest, TestGetFormat) {
  typedef draco::ImageFormat ImageFormat;
  ASSERT_EQ(draco::TextureUtils::GetFormat("png"), ImageFormat::PNG);
  ASSERT_EQ(draco::TextureUtils::GetFormat("jpg"), ImageFormat::JPEG);
  ASSERT_EQ(draco::TextureUtils::GetFormat("jpeg"), ImageFormat::JPEG);
  ASSERT_EQ(draco::TextureUtils::GetFormat("basis"), ImageFormat::BASIS);
  ASSERT_EQ(draco::TextureUtils::GetFormat("ktx2"), ImageFormat::BASIS);
  ASSERT_EQ(draco::TextureUtils::GetFormat("webp"), ImageFormat::WEBP);
  ASSERT_EQ(draco::TextureUtils::GetFormat(""), ImageFormat::NONE);
  ASSERT_EQ(draco::TextureUtils::GetFormat("bmp"), ImageFormat::NONE);
}

TEST(TextureUtilsTest, TestGetExtension) {
  typedef draco::ImageFormat ImageFormat;
  ASSERT_EQ(draco::TextureUtils::GetExtension(ImageFormat::PNG), "png");
  ASSERT_EQ(draco::TextureUtils::GetExtension(ImageFormat::JPEG), "jpg");
  ASSERT_EQ(draco::TextureUtils::GetExtension(ImageFormat::BASIS), "ktx2");
  ASSERT_EQ(draco::TextureUtils::GetExtension(ImageFormat::WEBP), "webp");
  ASSERT_EQ(draco::TextureUtils::GetExtension(ImageFormat::NONE), "");
}

#ifdef DRACO_SIMPLIFIER_SUPPORTED
TEST(TextureUtilsTest, TestHasTargetImageFormat) {
  // Tests that the presence of image format in texture library can be detected.

  // Create test texture library with textures in PNG and JPEG image formats.
  draco::TextureLibrary library;
  library.PushTexture(
      draco::ReadTextureFromFile(draco::GetTestFileFullPath("test.png"))
          .value());
  library.PushTexture(
      draco::ReadTextureFromFile(draco::GetTestFileFullPath("fast.jpg"))
          .value());

  // Check target texture image formats.
  ASSERT_TRUE(draco::TextureUtils::HasTargetImageFormat(
      library, draco::ImageFormat::PNG));
  ASSERT_TRUE(draco::TextureUtils::HasTargetImageFormat(
      library, draco::ImageFormat::JPEG));
  ASSERT_FALSE(draco::TextureUtils::HasTargetImageFormat(
      library, draco::ImageFormat::BASIS));
  ASSERT_FALSE(draco::TextureUtils::HasTargetImageFormat(
      library, draco::ImageFormat::WEBP));

  // Change PNG texture image format to BASIS and check again.
  library.GetTexture(0)->GetMutableCompressionOptions().target_image_format =
      draco::ImageFormat::BASIS;
  ASSERT_FALSE(draco::TextureUtils::HasTargetImageFormat(
      library, draco::ImageFormat::PNG));
  ASSERT_TRUE(draco::TextureUtils::HasTargetImageFormat(
      library, draco::ImageFormat::BASIS));
}

TEST(TextureUtilsTest, TestFindMutableTextures) {
  // Tests that all textures from material library can be found.

  // Read a mesh that has multiple textures of various types.
  std::unique_ptr<draco::Mesh> mesh =
      draco::ReadMeshFromTestFile("SphereAllSame/sphere_texture_all.gltf");
  ASSERT_NE(mesh, nullptr);
  draco::MaterialLibrary &library = mesh->GetMaterialLibrary();
  ASSERT_EQ(library.NumMaterials(), 1);
  const draco::Material &material = *library.GetMaterial(0);
  ASSERT_EQ(material.NumTextureMaps(), 5);
  ASSERT_EQ(library.GetTextureLibrary().NumTextures(), 4);

  // Check that all textures from material library can be found.
  const std::vector<draco::Texture *> textures =
      draco::TextureUtils::FindMutableTextures(&library);
  ASSERT_EQ(textures.size(), 4);
  ASSERT_EQ(textures[0], library.GetTextureLibrary().GetTexture(0));
  ASSERT_EQ(textures[1], library.GetTextureLibrary().GetTexture(1));
  ASSERT_EQ(textures[2], library.GetTextureLibrary().GetTexture(2));
  ASSERT_EQ(textures[3], library.GetTextureLibrary().GetTexture(3));
}

TEST(TextureUtilsTest, TestFindTexturesWithType) {
  // Tests that textures of a given type can be found.

  // Read a mesh that has multiple textures of various types.
  std::unique_ptr<draco::Mesh> mesh =
      draco::ReadMeshFromTestFile("SphereAllSame/sphere_texture_all.gltf");
  ASSERT_NE(mesh, nullptr);
  const draco::MaterialLibrary &library = mesh->GetMaterialLibrary();
  ASSERT_EQ(library.NumMaterials(), 1);
  const draco::Material &material = *library.GetMaterial(0);
  ASSERT_EQ(material.NumTextureMaps(), 5);
  ASSERT_EQ(library.GetTextureLibrary().NumTextures(), 4);

  // Check that occlusion texture can be found.
  {
    const std::vector<const draco::Texture *> textures =
        draco::TextureUtils::FindTextures(draco::TextureMap::AMBIENT_OCCLUSION,
                                          &library);
    ASSERT_EQ(textures.size(), 1);
    ASSERT_EQ(textures[0],
              library.GetMaterial(0)
                  ->GetTextureMapByType(draco::TextureMap::AMBIENT_OCCLUSION)
                  ->texture());
  }

  // Check that metallic texture can be found.
  {
    const std::vector<const draco::Texture *> textures =
        draco::TextureUtils::FindTextures(draco::TextureMap::METALLIC_ROUGHNESS,
                                          &library);
    ASSERT_EQ(textures.size(), 1);
    ASSERT_EQ(textures[0],
              library.GetMaterial(0)
                  ->GetTextureMapByType(draco::TextureMap::METALLIC_ROUGHNESS)
                  ->texture());
  }
}

TEST(TextureUtilsTest, TestFindMutableTexturesWithType) {
  // Tests that textures of a given type can be found.

  // Read a mesh that has multiple textures of various types.
  std::unique_ptr<draco::Mesh> mesh =
      draco::ReadMeshFromTestFile("SphereAllSame/sphere_texture_all.gltf");
  ASSERT_NE(mesh, nullptr);
  draco::MaterialLibrary &library = mesh->GetMaterialLibrary();
  ASSERT_EQ(library.NumMaterials(), 1);
  const draco::Material &material = *library.GetMaterial(0);
  ASSERT_EQ(material.NumTextureMaps(), 5);
  ASSERT_EQ(library.GetTextureLibrary().NumTextures(), 4);

  // Check that occlusion texture can be found.
  {
    const std::vector<draco::Texture *> textures =
        draco::TextureUtils::FindMutableTextures(
            draco::TextureMap::AMBIENT_OCCLUSION, &library);
    ASSERT_EQ(textures.size(), 1);
    ASSERT_EQ(textures[0],
              library.GetMaterial(0)
                  ->GetTextureMapByType(draco::TextureMap::AMBIENT_OCCLUSION)
                  ->texture());
  }

  // Check that metallic texture can be found.
  {
    const std::vector<draco::Texture *> textures =
        draco::TextureUtils::FindMutableTextures(
            draco::TextureMap::METALLIC_ROUGHNESS, &library);
    ASSERT_EQ(textures.size(), 1);
    ASSERT_EQ(textures[0],
              library.GetMaterial(0)
                  ->GetTextureMapByType(draco::TextureMap::METALLIC_ROUGHNESS)
                  ->texture());
  }
}
#endif  // DRACO_SIMPLIFIER_SUPPORTED

TEST(TextureUtilsTest, TestComputeRequiredNumChannels) {
  // Tests that the number of texture channels can be computed. Material library
  // under test is created programmatically.

  // Load textures.
  DRACO_ASSIGN_OR_ASSERT(
      auto texture0, draco::ReadTextureFromFile(
                         draco::GetTestFileFullPath("fully_transparent.png")));
  ASSERT_NE(texture0, nullptr);
  draco::Texture *texture0_ptr = texture0.get();
  DRACO_ASSIGN_OR_ASSERT(
      auto texture1,
      draco::ReadTextureFromFile(draco::GetTestFileFullPath("squares.png")));
  ASSERT_NE(texture1, nullptr);
  const draco::Texture *texture1_ptr = texture1.get();
  DRACO_ASSIGN_OR_ASSERT(
      auto texture2, draco::ReadTextureFromFile(
                         draco::GetTestFileFullPath("fully_transparent.png")));
  ASSERT_NE(texture2, nullptr);
  const draco::Texture *texture2_ptr = texture2.get();

  // Compute number of channels for occlusion-only texture.
  draco::MaterialLibrary library;
  draco::Material *const material0 = library.MutableMaterial(0);
  material0->SetTextureMap(std::move(texture0),
                           draco::TextureMap::AMBIENT_OCCLUSION, 0);
  ASSERT_EQ(
      draco::TextureUtils::ComputeRequiredNumChannels(*texture0_ptr, library),
      1);

  // Compute number of channels for occlusion-only texture with MR present but
  // not using the same texture.
  draco::Material *const material1 = library.MutableMaterial(1);
  material1->SetTextureMap(std::move(texture1),
                           draco::TextureMap::METALLIC_ROUGHNESS, 0);
  ASSERT_EQ(
      draco::TextureUtils::ComputeRequiredNumChannels(*texture0_ptr, library),
      1);

  // Compute number of channels for metallic-roughness texture.
  ASSERT_EQ(
      draco::TextureUtils::ComputeRequiredNumChannels(*texture1_ptr, library),
      3);

  // Compute number of channels texture that is used for occlusin map in one
  // material and also shared with metallic-roughness map in another material.
  draco::Material *const material2 = library.MutableMaterial(2);
  DRACO_ASSERT_OK(material2->SetTextureMap(
      texture0_ptr, draco::TextureMap::METALLIC_ROUGHNESS, 0));
  ASSERT_EQ(
      draco::TextureUtils::ComputeRequiredNumChannels(*texture0_ptr, library),
      3);

  // Compute number of channels for non-opaque texture.
  material0->SetTextureMap(std::move(texture2), draco::TextureMap::COLOR, 0);
  ASSERT_EQ(
      draco::TextureUtils::ComputeRequiredNumChannels(*texture2_ptr, library),
      4);
}

}  // namespace

#endif  // DRACO_TRANSCODER_SUPPORTED
