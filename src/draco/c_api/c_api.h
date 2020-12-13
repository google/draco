// Copyright 2020 The Draco Authors.
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
#ifndef DRACO_C_API_H_
#define DRACO_C_API_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// If compiling with Visual Studio.
#if defined(_MSC_VER)
#define EXPORT_API __declspec(dllexport)
#else
// Other platforms don't need this.
#define EXPORT_API
#endif  // defined(_MSC_VER)

typedef const char* draco_string; // NULL terminated

// draco::Status

typedef struct draco_status draco_status;

EXPORT_API void dracoStatusRelease(draco_status *status);

EXPORT_API int dracoStatusCode(const draco_status *status);

EXPORT_API bool dracoStatusOk(const draco_status *status);

// Returns the status message.
// The memory backing memory is valid meanwhile status is not released.
EXPORT_API draco_string dracoStatusErrorMsg(const draco_status *status);

// draco::Mesh

typedef uint32_t draco_face[3];
typedef struct draco_mesh draco_mesh;

EXPORT_API draco_mesh* dracoNewMesh();

EXPORT_API void dracoMeshRelease(draco_mesh *mesh);

EXPORT_API uint32_t dracoMeshNumFaces(const draco_mesh *mesh);

EXPORT_API uint32_t dracoMeshNumPoints(const draco_mesh *mesh);

// Queries an array of 3*face_count elements containing the triangle indices.
// out_values must be allocated to contain at least 3*face_count uint16_t elements.
// out_size must be exactly 3*face_count*sizeof(uint16_t), else out_values
// won´t be filled and returns false.
EXPORT_API bool dracoMeshGetTrianglesUint16(const draco_mesh *mesh,
                                            const size_t out_size,
                                            uint16_t *out_values);

// Queries an array of 3*face_count elements containing the triangle indices.
// out_values must be allocated to contain at least 3*face_count uint32_t elements.
// out_size must be exactly 3*face_count*sizeof(uint32_t), else out_values
// won´t be filled and returns false.
EXPORT_API bool dracoMeshGetTrianglesUint32(const draco_mesh *mesh,
                                            const size_t out_size,
                                            uint32_t *out_values);

// draco::Decoder

typedef struct draco_decoder draco_decoder;

EXPORT_API draco_decoder* dracoNewDecoder();

EXPORT_API void dracoDecoderRelease(draco_decoder *decoder);

EXPORT_API draco_status* dracoDecoderArrayToMesh(draco_decoder *decoder, 
                                                 const char *data, 
                                                 size_t data_size,
                                                 draco_mesh *out_mesh);

#ifdef __cplusplus
}
#endif

#endif  // DRACO_C_API_H_