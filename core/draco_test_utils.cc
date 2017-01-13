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
#include "core/draco_test_utils.h"

#include <fstream>

#include "core/macros.h"
#include "draco_test_base.h"

namespace draco {

namespace {
static constexpr char kTestDataDir[] = DRACO_TEST_DATA_DIR;
}  // namespace

std::string GetTestFileFullPath(const std::string &file_name) {
  return std::string(kTestDataDir) + std::string("/") + file_name;
}

bool GenerateGoldenFile(const std::string &golden_file_name, const void *data,
                        int data_size) {
  // TODO(ostava): This will work only when the test is executed locally
  // from blaze-bin/ folder. We should look for ways how to
  // make it work when it's run using the "blaze test" command.
  const std::string path = GetTestFileFullPath(golden_file_name);
  std::ofstream file(path, std::ios::binary);
  if (!file)
    return false;
  file.write(static_cast<const char *>(data), data_size);
  file.close();
  return true;
}

bool CompareGoldenFile(const std::string &golden_file_name, const void *data,
                       int data_size) {
  const std::string golden_path = GetTestFileFullPath(golden_file_name);
  std::ifstream in_file(golden_path);
  if (!in_file || data_size < 0)
    return false;
  const char *const data_c8 = static_cast<const char *>(data);
  constexpr int buffer_size = 1024;
  char buffer[buffer_size];
  size_t extracted_size = 0;
  size_t remaining_data_size = data_size;
  int offset = 0;
  while ((extracted_size = in_file.read(buffer, buffer_size).gcount()) > 0) {
    if (remaining_data_size <= 0)
      break;  // Input and golden sizes are different.
    size_t size_to_check = extracted_size;
    if (remaining_data_size < size_to_check)
      size_to_check = remaining_data_size;
    for (uint32_t i = 0; i < size_to_check; ++i) {
      if (buffer[i] != data_c8[offset++]) {
        LOG(INFO) << "Test output differed from golden file at byte "
                  << offset - 1;
        return false;
      }
    }
    remaining_data_size -= extracted_size;
  }
  if (remaining_data_size != extracted_size) {
    // Both of these values should be 0 at the end.
    LOG(INFO) << "Test output size differed from golden file size";
    return false;
  }
  return true;
}

}  // namespace draco
