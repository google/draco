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
#include "draco/maya/draco_maya_plugin.h"

#ifdef BUILD_MAYA_PLUGIN

namespace draco {
	namespace maya {

		static void decode_faces(std::unique_ptr<draco::Mesh> &drc_mesh, Drc2PyMesh* out_mesh) {
			int num_faces = drc_mesh->num_faces();
			out_mesh->faces = new int[num_faces*3];
			out_mesh->faces_num = num_faces;

			for (int i = 0; i < num_faces; i++) {
				const draco::Mesh::Face &face = drc_mesh->face(draco::FaceIndex(i));
				out_mesh->faces[i * 3 + 0] = face[0].value();
				out_mesh->faces[i * 3 + 1] = face[1].value();
				out_mesh->faces[i * 3 + 2] = face[2].value();				
			}
		}
		static void decode_vertices(std::unique_ptr<draco::Mesh> &drc_mesh, Drc2PyMesh* out_mesh) {
			int num_vertices = drc_mesh->num_points();
			out_mesh->vertices = new float[num_vertices * 3];
			out_mesh->vertices_num = num_vertices;

			const auto pos_att = drc_mesh->GetNamedAttribute(draco::GeometryAttribute::POSITION);
			for (int i = 0; i < num_vertices; i++) {
				draco::PointIndex pi(i);
				const draco::AttributeValueIndex val_index = pos_att->mapped_index(pi);

				float out_vertex[3];
				bool is_ok = pos_att->ConvertValue<float, 3>(val_index, out_vertex);
				if (!is_ok) return;

				out_mesh->vertices[i * 3 + 0] = out_vertex[0];
				out_mesh->vertices[i * 3 + 1] = out_vertex[1];
				out_mesh->vertices[i * 3 + 2] = out_vertex[2];
			}
		}

		static void decode_normals(std::unique_ptr<draco::Mesh> &drc_mesh, Drc2PyMesh* out_mesh) {
			const auto normal_att = drc_mesh->GetNamedAttribute(draco::GeometryAttribute::NORMAL);
			if (normal_att == nullptr) {
				out_mesh->normals = new float[0];
				out_mesh->normals_num = 0;
				return;
			}

			int num_normals = drc_mesh->num_points();
			out_mesh->normals = new float[num_normals];
			out_mesh->normals_num = num_normals;
		
			for (int i = 0; i < num_normals; i++) {
				draco::PointIndex pi(i);
				const draco::AttributeValueIndex val_index = normal_att->mapped_index(pi);

				float out_normal[3];
				bool is_ok = normal_att->ConvertValue<float, 3>(val_index, out_normal);
				if (!is_ok) return;

				out_mesh->normals[i * 3 + 0] = out_normal[0];
				out_mesh->normals[i * 3 + 1] = out_normal[1];
				out_mesh->normals[i * 3 + 2] = out_normal[2];
			}
		}

		static void decode_uvs(std::unique_ptr<draco::Mesh> &drc_mesh, Drc2PyMesh* out_mesh) {
			const auto uv_att = drc_mesh->GetNamedAttribute(draco::GeometryAttribute::TEX_COORD);
			if (uv_att == nullptr) {
				out_mesh->uvs = new float[0];
				out_mesh->uvs_num = 0;
				return;
			}

			int num_uvs = drc_mesh->num_points();
			out_mesh->normals = new float[num_uvs * 2];
			out_mesh->normals_num = num_uvs;

			for (int i = 0; i < num_uvs; i++) {
				draco::PointIndex pi(i);
				const draco::AttributeValueIndex val_index = uv_att->mapped_index(pi);

				float out_uv[3];
				bool is_ok = uv_att->ConvertValue<float, 3>(val_index, out_uv);
				if (!is_ok) return;

				out_mesh->uvs[i * 3 + 0] = out_uv[0];
				out_mesh->uvs[i * 3 + 1] = out_uv[1];
				out_mesh->uvs[i * 3 + 2] = out_uv[2];
			}
		}

		void drc2py_free(Drc2PyMesh **mesh_ptr) {
			Drc2PyMesh *mesh = *mesh_ptr;
			if (!mesh) return;
			if (mesh->faces) {
				delete[] mesh->faces;
				mesh->faces = nullptr;
				mesh->faces_num = 0;
			}
			if (mesh->vertices) {
				delete[] mesh->vertices;
				mesh->vertices = nullptr;
				mesh->vertices_num = 0;
			}
			if (mesh->normals) {
				delete[] mesh->normals;
				mesh->normals = nullptr;
				mesh->normals_num = 0;
			}
			delete mesh;
			*mesh_ptr = nullptr;
		}
	
		int drc2py_decode(char *data, unsigned int length, Drc2PyMesh **res_mesh) {
			draco::DecoderBuffer buffer;
			buffer.Init(data, length);
			auto type_statusor = draco::Decoder::GetEncodedGeometryType(&buffer);
			if (!type_statusor.ok()) {
				return -1;
			}
			const draco::EncodedGeometryType geom_type = type_statusor.value();
			if (geom_type != draco::TRIANGULAR_MESH) {
				return -2;
			}

			draco::Decoder decoder;
			auto statusor = decoder.DecodeMeshFromBuffer(&buffer);
			if (!statusor.ok()) {
				return -3;
			}
			std::unique_ptr<draco::Mesh> drc_mesh = std::move(statusor).value();

			*res_mesh = new Drc2PyMesh();
			decode_faces(drc_mesh, *res_mesh);
			decode_vertices(drc_mesh, *res_mesh);
			decode_normals(drc_mesh, *res_mesh);
			return 0;
		}
/*
		  // Get color attributes.
		  const auto color_att =
			  in_mesh->GetNamedAttribute(draco::GeometryAttribute::COLOR);
		  if (color_att != nullptr) {
			unity_mesh->color = new float[in_mesh->num_points() * 3];
			unity_mesh->has_color = true;
			for (draco::PointIndex i(0); i < in_mesh->num_points(); ++i) {
			  const draco::AttributeValueIndex val_index = color_att->mapped_index(i);
			  if (!color_att->ConvertValue<float, 3>(
					  val_index, unity_mesh->color + i.value() * 3)) {
				ReleaseMayaMesh(&unity_mesh);
				return -8;
			  }
			}
		  }
		  // Get texture coordinates attributes.
		  const auto texcoord_att =
			  in_mesh->GetNamedAttribute(draco::GeometryAttribute::TEX_COORD);
		  if (texcoord_att != nullptr) {
			unity_mesh->texcoord = new float[in_mesh->num_points() * 2];
			unity_mesh->has_texcoord = true;
			for (draco::PointIndex i(0); i < in_mesh->num_points(); ++i) {
			  const draco::AttributeValueIndex val_index =
				  texcoord_att->mapped_index(i);
			  if (!texcoord_att->ConvertValue<float, 3>(
					  val_index, unity_mesh->texcoord + i.value() * 3)) {
				ReleaseMayaMesh(&unity_mesh);
				return -8;
			  }
			}
		  }
*/
}  // namespace maya

}  // namespace draco

#endif  // BUILD_MAYA_PLUGIN
