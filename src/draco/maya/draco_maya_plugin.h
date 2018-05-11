// Copyright 2017 The Draco Authors.
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
			struct EXPORT_API DracoToMayaMesh {
				DracoToMayaMesh()
					: num_faces(0),
					indices(nullptr),
					num_vertices(0),
					position(nullptr),
					has_normal(false),
					normal(nullptr),
					has_texcoord(false),
					texcoord(nullptr),
					has_color(false),
					color(nullptr) {}

				int num_faces;
				int *indices;
				int num_vertices;
				float *position;
				bool has_normal;
				float *normal;
				bool has_texcoord;
				float *texcoord;
				bool has_color;
				float *color;
			};

			void ReleaseMayaMesh(DracoToMayaMesh **mesh_ptr);

			/* To use this function, you do not allocate memory for |tmp_mesh|, just
			 * define and pass a null pointer. Otherwise there will be memory leak.
			 */
			int EXPORT_API DecodeMeshForMaya(char *data, unsigned int length, DracoToMayaMesh **tmp_mesh);


			struct EXPORT_API Drc2PyMesh {
				Drc2PyMesh()
					: faces_num(0),
					  vertices_num(0) {}
				int faces_num;
				int* faces;
				int vertices_num;
				float* vertices;
			};

			EXPORT_API int drc2py_decode(char *data, unsigned int length, Drc2PyMesh **res_mesh);



			struct EXPORT_API MyStruct {
				MyStruct()
					: num_faces(0) {}
					//indices(nullptr) {}
				int num_faces;
				//int *indices;
			};
			int EXPORT_API hello();
			void EXPORT_API fill_int(int *num);

			EXPORT_API MyStruct* fill_mystruct();
			EXPORT_API void fill_mystruct2(MyStruct** my);

		}  // extern "C"

	} // namespace maya
}  // namespace draco

#endif  // BUILD_MAYA_PLUGIN

#endif  // DRACO_MAYA_PLUGIN_H_
