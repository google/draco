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

#include "draco/unity/draco_unity_enc_plugin.h"

#ifdef DRACO_UNITY_PLUGIN

namespace draco {

  void* EXPORT_API CreateDracoMeshEncoder( uint32_t faceCount ) {
    auto mesh_builder = new TriangleSoupMeshBuilder();
    mesh_builder->Start(faceCount);
    return mesh_builder;
  }

  int EXPORT_API DracoMeshAddAttribute(void * dracoMesh, int attributeType, DataType dataType, int numComponents) {
    TriangleSoupMeshBuilder *const mesh_builder = static_cast<TriangleSoupMeshBuilder *>(dracoMesh);
    return mesh_builder->AddAttribute((GeometryAttribute::Type)attributeType, numComponents, dataType);
  }

  void EXPORT_API DracoMeshAddFaceValues(void * dracoMesh, int faceIndex, int attributeId, int numComponents, const char* data0, const char* data1, const char* data2) {
    TriangleSoupMeshBuilder *const mesh_builder = static_cast<TriangleSoupMeshBuilder *>(dracoMesh);
    mesh_builder->SetAttributeValuesForFace(attributeId, draco::FaceIndex(faceIndex), data0, data1, data2);
  }

  void EXPORT_API DracoMeshFinalize(void* dracoMesh, void** bufferPtr, const char** result, int* size) {
    TriangleSoupMeshBuilder *const mesh_builder = static_cast<TriangleSoupMeshBuilder *>(dracoMesh);
    auto mesh = mesh_builder->Finalize();
    draco::ExpertEncoder encoder(*mesh);

    // TODO: set quantization parameters
    // encoder.SetAttributeQuantization(0, 16);  // Position quantization.
    // encoder.SetAttributeQuantization(1, 15);  // Tex-coord 0 quantization.
    // encoder.SetAttributeQuantization(2, 14);  // Tex-coord 1 quantization.

    auto buffer = new EncoderBuffer();
    encoder.EncodeToBuffer(buffer);

    delete mesh_builder;

    *bufferPtr = buffer;
    *result = buffer->data();
    *size = buffer->size();
  }

  void EXPORT_API ReleaseDracoMeshBuffer(void * bufferPtr) {
    EncoderBuffer *const buffer = static_cast<EncoderBuffer *>(bufferPtr);
    delete buffer;
  }

}  // namespace draco

#endif  // DRACO_UNITY_PLUGIN
