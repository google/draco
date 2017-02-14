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
#include <cinttypes>
#include <cstdlib>
#include <fstream>

#include "compression/encode.h"
#include "core/cycle_timer.h"
#include "io/mesh_io.h"
#include "io/point_cloud_io.h"

namespace {

struct Options {
  Options();

  bool is_point_cloud;
  int pos_quantization_bits;
  int tex_coords_quantization_bits;
  int normals_quantization_bits;
  int compression_level;
  std::string input;
  std::string output;
};

Options::Options()
    : is_point_cloud(false),
      pos_quantization_bits(14),
      tex_coords_quantization_bits(12),
      normals_quantization_bits(10),
      compression_level(5) {}

void Usage() {
  printf("Usage: draco_encoder [options] -i input\n");
  printf("\n");
  printf("Main options:\n");
  printf("  -h | -?               show help.\n");
  printf("  -i <input>            input file name.\n");
  printf("  -o <output>           output file name.\n");
  printf(
      "  -point_cloud          forces the input to be encoded as a point "
      "cloud.\n");
  printf(
      "  -qp <value>           quantization bits for the position "
      "attribute, default=14.\n");
  printf(
      "  -qt <value>           quantization bits for the texture coordinate "
      "attribute, default=12.\n");
  printf(
      "  -qn <value>           quantization bits for the normal vector "
      "attribute, default=10.\n");
  printf(
      "  -cl <value>           compression level [0-10], most=10, least=0, "
      "default=5.\n");
}

int StringToInt(const std::string &s) {
  char *end;
  return strtol(s.c_str(), &end, 10);  // NOLINT
}

void PrintOptions(const draco::PointCloud &pc, const Options &options) {
  printf("Encoder options:\n");
  printf("  Compression level = %d\n", options.compression_level);
  if (options.pos_quantization_bits <= 0) {
    printf("  Positions: No quantization\n");
  } else {
    printf("  Positions: Quantization = %d bits\n",
           options.pos_quantization_bits);
  }

  if (pc.GetNamedAttributeId(draco::GeometryAttribute::TEX_COORD) >= 0) {
    if (options.tex_coords_quantization_bits <= 0) {
      printf("  Texture coordinates: No quantization\n");
    } else {
      printf("  Texture coordinates: Quantization = %d bits\n",
             options.tex_coords_quantization_bits);
    }
  }

  if (pc.GetNamedAttributeId(draco::GeometryAttribute::NORMAL) >= 0) {
    if (options.normals_quantization_bits <= 0) {
      printf("  Normals: No quantization\n");
    } else {
      printf("  Normals: Quantization = %d bits\n",
             options.normals_quantization_bits);
    }
  }
  printf("\n");
}

int EncodePointCloudToFile(const draco::PointCloud &pc,
                           const draco::EncoderOptions &options,
                           const std::string &file) {
  draco::CycleTimer timer;
  // Encode the geometry.
  draco::EncoderBuffer buffer;
  timer.Start();
  if (!draco::EncodePointCloudToBuffer(pc, options, &buffer)) {
    printf("Failed to encode the point cloud.\n");
    return -1;
  }
  timer.Stop();
  // Save the encoded geometry into a file.
  std::ofstream out_file(file, std::ios::binary);
  if (!out_file) {
    printf("Failed to create the output file.\n");
    return -1;
  }
  out_file.write(buffer.data(), buffer.size());
  printf("Encoded point cloud saved to %s (%" PRId64 " ms to encode)\n",
         file.c_str(), timer.GetInMs());
  printf("\nEncoded size = %zu bytes\n\n", buffer.size());
  return 0;
}

int EncodeMeshToFile(const draco::Mesh &mesh,
                     const draco::EncoderOptions &options,
                     const std::string &file) {
  draco::CycleTimer timer;
  // Encode the geometry.
  draco::EncoderBuffer buffer;
  timer.Start();
  if (!draco::EncodeMeshToBuffer(mesh, options, &buffer)) {
    printf("Failed to encode the mesh.\n");
    return -1;
  }
  timer.Stop();
  // Save the encoded geometry into a file.
  std::ofstream out_file(file, std::ios::binary);
  if (!out_file) {
    printf("Failed to create the output file.\n");
    return -1;
  }
  out_file.write(buffer.data(), buffer.size());
  printf("Encoded mesh saved to %s (%" PRId64 " ms to encode)\n", file.c_str(),
         timer.GetInMs());
  printf("\nEncoded size = %zu bytes\n\n", buffer.size());
  return 0;
}

}  // anonymous namespace

int main(int argc, char **argv) {
  Options options;
  const int argc_check = argc - 1;

  for (int i = 1; i < argc; ++i) {
    if (!strcmp("-h", argv[i]) || !strcmp("-?", argv[i])) {
      Usage();
      return 0;
    } else if (!strcmp("-i", argv[i]) && i < argc_check) {
      options.input = argv[++i];
    } else if (!strcmp("-o", argv[i]) && i < argc_check) {
      options.output = argv[++i];
    } else if (!strcmp("-point_cloud", argv[i])) {
      options.is_point_cloud = true;
    } else if (!strcmp("-qp", argv[i]) && i < argc_check) {
      options.pos_quantization_bits = StringToInt(argv[++i]);
      if (options.pos_quantization_bits > 31) {
        printf(
            "Error: The maximum number of quantization bits for the position "
            "attribute is 31.\n");
        return -1;
      }
    } else if (!strcmp("-qt", argv[i]) && i < argc_check) {
      options.tex_coords_quantization_bits = StringToInt(argv[++i]);
      if (options.tex_coords_quantization_bits > 31) {
        printf(
            "Error: The maximum number of quantization bits for the texture "
            "coordinate attribute is 31.\n");
        return -1;
      }
    } else if (!strcmp("-qn", argv[i]) && i < argc_check) {
      options.normals_quantization_bits = StringToInt(argv[++i]);
      if (options.normals_quantization_bits > 31) {
        printf(
            "Error: The maximum number of quantization bits for the normal "
            "attribute is 31.\n");
        return -1;
      }
    } else if (!strcmp("-cl", argv[i]) && i < argc_check) {
      options.compression_level = StringToInt(argv[++i]);
    }
  }
  if (argc < 3 || options.input.empty()) {
    Usage();
    return -1;
  }

  std::unique_ptr<draco::PointCloud> pc;
  draco::Mesh *mesh = nullptr;
  if (!options.is_point_cloud) {
    std::unique_ptr<draco::Mesh> in_mesh =
        draco::ReadMeshFromFile(options.input);
    if (!in_mesh) {
      printf("Failed loading the input mesh.\n");
      return -1;
    }
    mesh = in_mesh.get();
    pc = std::move(in_mesh);
  } else {
    pc = draco::ReadPointCloudFromFile(options.input);
    if (!pc) {
      printf("Failed loading the input point cloud.\n");
      return -1;
    }
  }

  // Setup encoder options.
  draco::EncoderOptions encoder_options = draco::CreateDefaultEncoderOptions();
  if (options.pos_quantization_bits > 0) {
    draco::SetNamedAttributeQuantization(&encoder_options, *pc.get(),
                                         draco::GeometryAttribute::POSITION,
                                         options.pos_quantization_bits);
  }
  if (options.tex_coords_quantization_bits > 0) {
    draco::SetNamedAttributeQuantization(&encoder_options, *pc.get(),
                                         draco::GeometryAttribute::TEX_COORD,
                                         options.tex_coords_quantization_bits);
  }
  if (options.normals_quantization_bits > 0) {
    draco::SetNamedAttributeQuantization(&encoder_options, *pc.get(),
                                         draco::GeometryAttribute::NORMAL,
                                         options.normals_quantization_bits);
  }
  // Convert compression level to speed (that 0 = slowest, 10 = fastest).
  const int speed = 10 - options.compression_level;
  draco::SetSpeedOptions(&encoder_options, speed, speed);

  if (options.output.empty()) {
    // Create a default output file by attaching .drc to the input file name.
    options.output = options.input + ".drc";
  }

  PrintOptions(*pc.get(), options);
  if (mesh && mesh->num_faces() > 0)
    return EncodeMeshToFile(*mesh, encoder_options, options.output);
  return EncodePointCloudToFile(*pc.get(), encoder_options, options.output);
}
