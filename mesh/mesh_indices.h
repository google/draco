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
#ifndef DRACO_MESH_MESH_INDICES_H_
#define DRACO_MESH_MESH_INDICES_H_

#include "point_cloud/geometry_indices.h"

namespace draco {

// Face index used in a corner table.
DEFINE_NEW_DRACO_INDEX_TYPE(int32_t, FaceIndex);

// Constants denoting invalid indices.
static constexpr FaceIndex kInvalidFaceIndex(
    std::numeric_limits<int32_t>::min() / 2);

}  // namespace draco

#endif  // DRACO_MESH_MESH_INDICES_H_
