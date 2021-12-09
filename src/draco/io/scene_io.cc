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
#include "draco/io/scene_io.h"

#ifdef DRACO_TRANSCODER_SUPPORTED
#include "draco/io/gltf_decoder.h"
#include "draco/io/gltf_encoder.h"

namespace draco {

enum SceneFileFormat { UNKNOWN, GLTF, USD };

SceneFileFormat GetSceneFileFormat(const std::string &file_name) {
  const std::string extension = LowercaseFileExtension(file_name);
  if (extension == "gltf" || extension == "glb") {
    return GLTF;
  }
  if (extension == "usd" || extension == "usda" || extension == "usdc" ||
      extension == "usdz") {
    return USD;
  }
  return UNKNOWN;
}

StatusOr<std::unique_ptr<Scene>> ReadSceneFromFile(
    const std::string &file_name) {
  return ReadSceneFromFile(file_name, nullptr);
}

StatusOr<std::unique_ptr<Scene>> ReadSceneFromFile(
    const std::string &file_name, std::vector<std::string> *scene_files) {
  std::unique_ptr<Scene> scene(new Scene());
  switch (GetSceneFileFormat(file_name)) {
    case GLTF: {
      GltfDecoder decoder;
      return decoder.DecodeFromFileToScene(file_name, scene_files);
    }
    case USD: {
      return Status(Status::DRACO_ERROR, "USD is not supported yet.");
    }
    default: {
      return Status(Status::DRACO_ERROR, "Unknown input file format.");
    }
  }
}

Status WriteSceneToFile(const std::string &file_name, const Scene &scene) {
  Options options;
  return WriteSceneToFile(file_name, scene, options);
}

Status WriteSceneToFile(const std::string &file_name, const Scene &scene,
                        const Options &options) {
  const std::string extension = LowercaseFileExtension(file_name);
  std::string folder_path;
  std::string out_file_name;
  draco::SplitPath(file_name, &folder_path, &out_file_name);
  switch (GetSceneFileFormat(file_name)) {
    case GLTF: {
      GltfEncoder encoder;
      if (!encoder.EncodeToFile(scene, file_name, folder_path)) {
        return Status(Status::DRACO_ERROR, "Failed to encode the scene.");
      }
      return OkStatus();
    }
    case USD: {
      return Status(Status::DRACO_ERROR, "USD is not supported yet.");
    }
    default: {
      return Status(Status::DRACO_ERROR, "Unknown output file format.");
    }
  }
}

}  // namespace draco

#endif  // DRACO_TRANSCODER_SUPPORTED
