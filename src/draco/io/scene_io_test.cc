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
#include "draco/core/draco_test_utils.h"
#include "draco/io/file_utils.h"

namespace {

TEST(SceneTest, TestSceneIO) {
  // A simple test that verifies that the scene is loaded and saved using the
  // scene_io.h API.
  const std::string file_name =
      draco::GetTestFileFullPath("CesiumMilkTruck/glTF/CesiumMilkTruck.gltf");
  draco::StatusOr<std::unique_ptr<draco::Scene>> maybe_scene =
      draco::ReadSceneFromFile(file_name);
  ASSERT_TRUE(maybe_scene.status().ok());
  std::unique_ptr<draco::Scene> scene = std::move(maybe_scene).value();
  ASSERT_NE(scene, nullptr);

  const std::string out_file_name =
      draco::GetTestTempFileFullPath("out_scene.gltf");
  ASSERT_TRUE(draco::WriteSceneToFile(out_file_name, *scene).ok());

  // Ensure all files related to the scene are saved.
  ASSERT_GT(draco::GetFileSize(out_file_name), 0);
  ASSERT_GT(
      draco::GetFileSize(draco::GetTestTempFileFullPath("CesiumMilkTruck.png")),
      0);
  ASSERT_GT(draco::GetFileSize(draco::GetTestTempFileFullPath("buffer0.bin")),
            0);
}

}  // namespace
#endif  // DRACO_TRANSCODER_SUPPORTED
