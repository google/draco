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
#include "draco/mesh/mesh_misc_functions.h"

namespace draco {

std::unique_ptr<CornerTable> CreateCornerTable(const Mesh *mesh) {
  typedef CornerTable CT;

  const PointAttribute *const att =
      mesh->GetNamedAttribute(GeometryAttribute::POSITION);
  if (att == nullptr)
    return nullptr;
  IndexTypeVector<FaceIndex, FaceType> faces(mesh->num_faces());
  FaceType new_face;
  for (FaceIndex i(0); i < mesh->num_faces(); ++i) {
    const Mesh::Face &face = mesh->face(i);
    for (int j = 0; j < 3; ++j) {
      // Map general vertex indices to position indices.
      new_face[j] = att->mapped_index(face[j]).value();
    }
    faces[FaceIndex(i)] = new_face;
  }
  // Build the corner table.
  return CT::Create(faces);
}

std::unique_ptr<CornerTable> CreateCornerTableFromAllAttributes(
    const Mesh *mesh) {
  IndexTypeVector<FaceIndex, FaceType> faces(mesh->num_faces());
  FaceType new_face;
  for (FaceIndex i(0); i < mesh->num_faces(); ++i) {
    const Mesh::Face &face = mesh->face(i);
    // Each face is identified by point indices that automatically split the
    // mesh along attribute seams.
    for (int j = 0; j < 3; ++j) {
      new_face[j] = face[j].value();
    }
    faces[i] = new_face;
  }
  // Build the corner table.
  return CornerTable::Create(faces);
}

PointIndex CornerToPointId(CornerIndex ci, const CornerTable *ct,
                           const Mesh *mesh) {
  if (!ct->IsValid(ci))
    return kInvalidPointIndex;

  // Get Face index and local corner index for corner.
  const FaceIndex fi = ct->Face(ci);
  const int lci = ct->LocalIndex(ci);
  // Get the point id.
  return mesh->face(fi)[lci];
}

PointIndex CornerToPointId(int c, const CornerTable *ct, const Mesh *mesh) {
  return CornerToPointId(CornerIndex(c), ct, mesh);
}

}  // namespace draco
