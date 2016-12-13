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
// This file contains misc functions that are needed by several mesh related
// algorithms.

#ifndef DRACO_MESH_MESH_MISC_FUNCTIONS_H_
#define DRACO_MESH_MESH_MISC_FUNCTIONS_H_

#include "mesh/corner_table.h"
#include "mesh/mesh.h"

namespace draco {

// Creates a CornerTable from |*mesh|. Returns nullptr on error.
std::unique_ptr<CornerTable> CreateCornerTable(const Mesh *mesh);

// Returns the point id stored on corner |ci|.
PointIndex CornerToPointId(CornerIndex ci, const CornerTable *ct,
                           const Mesh *mesh);

// Returns the point id stored on corner |c|.
PointIndex CornerToPointId(int c, const CornerTable *ct, const Mesh *mesh);

// Returns the point id of |c| without using a corner table.
inline PointIndex CornerToPointId(int c, const Mesh *mesh) {
  return mesh->face(FaceIndex(c / 3))[c % 3];
}

}  // namespace draco

#endif  // DRACO_MESH_MESH_MISC_FUNCTIONS_H_
