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


		int hello() {
			return 20;
		}

		void fill_int(int *num) {
			*num = 1982;
		}

		MyStruct* fill_mystruct() {
			MyStruct* my = new MyStruct();
			my->num_faces = 500;
			return my;
		}

		void fill_mystruct2(MyStruct** my) {
			*my = new MyStruct();
			(*my)->num_faces = 900;
		}

		void ReleaseMayaMesh(DracoToMayaMesh **mesh_ptr) {
		  DracoToMayaMesh *mesh = *mesh_ptr;
		  if (!mesh)
			return;
		  if (mesh->indices) {
			delete[] mesh->indices;
			mesh->indices = nullptr;
		  }
		  if (mesh->position) {
			delete[] mesh->position;
			mesh->position = nullptr;
		  }
		  if (mesh->has_normal && mesh->normal) {
			delete[] mesh->normal;
			mesh->has_normal = false;
			mesh->normal = nullptr;
		  }
		  if (mesh->has_texcoord && mesh->texcoord) {
			delete[] mesh->texcoord;
			mesh->has_texcoord = false;
			mesh->texcoord = nullptr;
		  }
		  if (mesh->has_color && mesh->color) {
			delete[] mesh->color;
			mesh->has_color = false;
			mesh->color = nullptr;
		  }
		  delete mesh;
		  *mesh_ptr = nullptr;
		}

		int drc2py_decode(char *data, unsigned int length, Drc2PyMesh **res_mesh) {
			draco::DecoderBuffer buffer;
			buffer.Init(data, length);
			auto type_statusor = draco::Decoder::GetEncodedGeometryType(&buffer);
			if (!type_statusor.ok()) {
				// TODO(zhafang): Use enum instead.
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
			//(*res_mesh)->faces_num = drc_mesh->num_faces();
			//(*res_mesh)->vertices_num = drc_mesh->num_points();
			decode_faces(drc_mesh, *res_mesh);
			decode_vertices(drc_mesh, *res_mesh);

			return 0;
		}

		int DecodeMeshForMaya(char *data, unsigned int length, DracoToMayaMesh **tmp_mesh) {
		  draco::DecoderBuffer buffer;
		  buffer.Init(data, length);
		  auto type_statusor = draco::Decoder::GetEncodedGeometryType(&buffer);
		  if (!type_statusor.ok()) {
			// TODO(zhafang): Use enum instead.
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
		  std::unique_ptr<draco::Mesh> in_mesh = std::move(statusor).value();

		  *tmp_mesh = new DracoToMayaMesh();
		  DracoToMayaMesh *unity_mesh = *tmp_mesh;
		  unity_mesh->num_faces = in_mesh->num_faces();
		  unity_mesh->num_vertices = in_mesh->num_points();

		  unity_mesh->indices = new int[in_mesh->num_faces() * 3];
		  for (draco::FaceIndex face_id(0); face_id < in_mesh->num_faces(); ++face_id) {
			const Mesh::Face &face = in_mesh->face(draco::FaceIndex(face_id));
			memcpy(unity_mesh->indices + face_id.value() * 3,
				   reinterpret_cast<const int *>(face.data()), sizeof(int) * 3);
		  }

		  // TODO(zhafang): Add other attributes.
		  unity_mesh->position = new float[in_mesh->num_points() * 3];
		  const auto pos_att =
			  in_mesh->GetNamedAttribute(draco::GeometryAttribute::POSITION);
		  for (draco::PointIndex i(0); i < in_mesh->num_points(); ++i) {
			const draco::AttributeValueIndex val_index = pos_att->mapped_index(i);
			if (!pos_att->ConvertValue<float, 3>(
					val_index, unity_mesh->position + i.value() * 3)) {
			  ReleaseMayaMesh(&unity_mesh);
			  return -8;
			}
		  }
		  // Get normal attributes.
		  const auto normal_att =
			  in_mesh->GetNamedAttribute(draco::GeometryAttribute::NORMAL);
		  if (normal_att != nullptr) {
			unity_mesh->normal = new float[in_mesh->num_points() * 3];
			unity_mesh->has_normal = true;
			for (draco::PointIndex i(0); i < in_mesh->num_points(); ++i) {
			  const draco::AttributeValueIndex val_index = normal_att->mapped_index(i);
			  if (!normal_att->ConvertValue<float, 3>(
					  val_index, unity_mesh->normal + i.value() * 3)) {
				ReleaseMayaMesh(&unity_mesh);
				return -8;
			  }
			}
		  }
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

		  return in_mesh->num_faces();
		}

}  // namespace maya

}  // namespace draco

#endif  // BUILD_MAYA_PLUGIN
