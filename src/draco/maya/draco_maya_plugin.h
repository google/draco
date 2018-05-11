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
#ifndef DRACO_MAYA_PLUGIN_H_
#define DRACO_MAYA_PLUGIN_H_

#include "draco/compression/config/compression_shared.h"
#include "draco/compression/decode.h"

#ifdef BUILD_MAYA_PLUGIN

// If compiling with Visual Studio.
#if defined(_MSC_VER)
#define EXPORT_API __declspec(dllexport)
#else
// Other platforms don't need this.
#define EXPORT_API
#endif  // defined(_MSC_VER)

namespace draco {
	namespace maya {
		extern "C" {
			//void ReleaseMayaMesh(DracoToMayaMesh **mesh_ptr);

			struct EXPORT_API Drc2PyMesh {
				Drc2PyMesh()
					: faces_num(0),
					  faces(nullptr),
					  vertices_num(0),
					  vertices(nullptr),
					  normals_num(0),
					  normals(nullptr) {}
				int faces_num;
				int* faces;
				int vertices_num;
				float* vertices;
				int normals_num;
				float* normals;
			};

			EXPORT_API int drc2py_decode(char *data, unsigned int length, Drc2PyMesh **res_mesh);
		}  // extern "C"

	} // namespace maya
}  // namespace draco

#endif  // BUILD_MAYA_PLUGIN

#endif  // DRACO_MAYA_PLUGIN_H_
