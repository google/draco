// Copyright 2021 The Draco Authors.
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
#ifndef DRACO_UNITY_ENC_DRACO_UNITY_PLUGIN_H_
#define DRACO_UNITY_ENC_DRACO_UNITY_PLUGIN_H_

#ifdef DRACO_UNITY_PLUGIN

// If compiling with Visual Studio.
#if defined(_MSC_VER)
#define EXPORT_API __declspec(dllexport)
#else
// Other platforms don't need this.
#define EXPORT_API
#endif  // defined(_MSC_VER)

namespace draco {

extern "C" {

  void* EXPORT_API CreateDracoMeshEncoder( uint32_t faceCount );
  int EXPORT_API DracoMeshAddAttribute(void * dracoMesh, int attributeType, DataType dataType, int numComponents);
  void EXPORT_API DracoMeshAddFaceValues(void * dracoMesh, int faceIndex, int attributeId, int numComponents, const char* data0, const char* data1, const char* data2);
  void EXPORT_API DracoMeshFinalize(void* dracoMesh, void** bufferPtr, const char** result, int* size);
  void EXPORT_API ReleaseDracoMeshBuffer(void * bufferPtr);
}  // extern "C"

}  // namespace draco

#endif  // DRACO_UNITY_PLUGIN

#endif  // DRACO_UNITY_ENC_DRACO_UNITY_PLUGIN_H_
