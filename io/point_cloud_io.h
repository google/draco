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
#ifndef DRACO_IO_POINT_CLOUD_IO_H_
#define DRACO_IO_POINT_CLOUD_IO_H_

#include "compression/config/compression_shared.h"
#include "compression/decode.h"
#include "compression/encode.h"

namespace draco {

template <typename OutStreamT>
OutStreamT WritePointCloudIntoStream(const PointCloud *pc, OutStreamT &&os,
                                     PointCloudEncodingMethod method,
                                     const EncoderOptions &options) {
  EncoderBuffer buffer;
  EncoderOptions local_options = options;
  local_options.SetGlobalInt("encoding_method", method);
  if (!EncodePointCloudToBuffer(*pc, local_options, &buffer)) {
    os.setstate(std::ios_base::badbit);
    return os;
  }

  os.write(static_cast<const char *>(buffer.data()), buffer.size());

  return os;
}

template <typename OutStreamT>
OutStreamT WritePointCloudIntoStream(const PointCloud *pc, OutStreamT &&os,
                                     PointCloudEncodingMethod method) {
  const EncoderOptions options = CreateDefaultEncoderOptions();
  return WritePointCloudIntoStream(pc, os, method, options);
}

template <typename OutStreamT>
OutStreamT &WritePointCloudIntoStream(const PointCloud *pc, OutStreamT &&os) {
  return WritePointCloudIntoStream(pc, os, POINT_CLOUD_SEQUENTIAL_ENCODING);
}

template <typename InStreamT>
InStreamT &ReadPointCloudFromStream(std::unique_ptr<PointCloud> *point_cloud,
                                    InStreamT &&is) {
  // Determine size of stream and write into a vector
  const auto start_pos = is.tellg();
  is.seekg(0, std::ios::end);
  const std::streampos is_size = is.tellg() - start_pos;
  is.seekg(start_pos);
  std::vector<char> data(is_size);
  is.read(&data[0], is_size);

  // Create a point cloud from that data.
  DecoderBuffer buffer;
  buffer.Init(&data[0], data.size());
  *point_cloud = DecodePointCloudFromBuffer(&buffer);

  if (*point_cloud == nullptr) {
    is.setstate(std::ios_base::badbit);
  }
  return is;
}

// Reads a point cloud from a file. The function automatically chooses the
// correct decoder based on the extension of the files. Currently, .obj and .ply
// files are supported. Other file extensions are processed by the default
// draco::PointCloudDecoder.
// Returns nullptr if the decoding failed.
std::unique_ptr<PointCloud> ReadPointCloudFromFile(
    const std::string &file_name);

}  // namespace draco

#endif  // DRACO_IO_POINT_CLOUD_IO_H_
