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
#ifndef DRACO_COMPRESSION_DECODE_H_
#define DRACO_COMPRESSION_DECODE_H_

#include "compression/config/compression_shared.h"
#include "core/decoder_buffer.h"
#include "mesh/mesh.h"

namespace draco {

// Returns the geometry type encoded in the input |in_buffer|.
// The return value is one of POINT_CLOUD, MESH or INVALID_GEOMETRY in case
// the input data is invalid.
// The decoded geometry type can be used to choose an appropriate decoding
// function for a given geometry type (see below).
EncodedGeometryType GetEncodedGeometryType(DecoderBuffer *in_buffer);

// Decodes point cloud from the provided buffer. The buffer must be filled with
// data that was encoded with either the EncodePointCloudToBuffer or
// EncodeMeshToBuffer methods in encode.h. In case the input buffer contains
// mesh, the returned instance can be down-casted to Mesh.
std::unique_ptr<PointCloud> DecodePointCloudFromBuffer(
    DecoderBuffer *in_buffer);

// Decodes a triangular mesh from the provided buffer. The mesh must be filled
// with data that was encoded using the EncodeMeshToBuffer method in encode.h.
// The function will return nullptr in case the input is invalid or if it was
// encoded with the EncodePointCloudToBuffer method.
std::unique_ptr<Mesh> DecodeMeshFromBuffer(DecoderBuffer *in_buffer);

}  // namespace draco

#endif  // DRACO_COMPRESSION_DECODE_H_
