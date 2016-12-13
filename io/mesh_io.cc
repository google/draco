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
#include "io/mesh_io.h"

#include <fstream>

#include "io/obj_decoder.h"
#include "io/parser_utils.h"
#include "io/ply_decoder.h"

namespace draco {

std::unique_ptr<Mesh> ReadMeshFromFile(const std::string &file_name) {
  std::unique_ptr<Mesh> mesh(new Mesh());
  // Analyze file extension.
  const std::string extension =
      parser::ToLower(file_name.substr(file_name.size() - 4));
  if (extension == ".obj") {
    // Wavefront OBJ file format.
    ObjDecoder obj_decoder;
    if (!obj_decoder.DecodeFromFile(file_name, mesh.get()))
      return nullptr;
    return mesh;
  }
  if (extension == ".ply") {
    // Wavefront PLY file format.
    PlyDecoder ply_decoder;
    if (!ply_decoder.DecodeFromFile(file_name, mesh.get()))
      return nullptr;
    return mesh;
  }

  // Otherwise not an obj file. Assume the file was encoded with one of the
  // draco encoding methods.
  std::ifstream is(file_name.c_str(), std::ios::binary);
  if (!is)
    return nullptr;
  if (!ReadMeshFromStream(&mesh, is).good())
    return nullptr;  // Error reading the stream.
  return mesh;
}

}  // namespace draco
