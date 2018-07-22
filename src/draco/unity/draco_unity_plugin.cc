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
#include "draco/unity/draco_unity_plugin.h"
#include "draco/io/obj_decoder.h"

#ifdef BUILD_UNITY_PLUGIN

namespace draco {

void ReleaseUnityMesh(DracoToUnityMesh **mesh_ptr) {
  DracoToUnityMesh *mesh = *mesh_ptr;
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

  if (mesh->num_submesh && mesh->submesh)
  {
	  for (int i = 0; i < mesh->num_submesh; i++)
	  {
		  delete[] mesh->submesh[i];
	  }
	  delete[] mesh->submesh;
  }

  delete mesh;
  *mesh_ptr = nullptr;
}

int EncodeMeshForObjBuff(char **data, char *ObjBuff,int BufSize)
{
	draco::ObjDecoder obj_decoder;
	obj_decoder.set_use_metadata(false);
	draco::DecoderBuffer decBuff;
	decBuff.Init(ObjBuff, BufSize);
	draco::Mesh encodeMesh;

	const Status obj_status = obj_decoder.DecodeFromBuffer(&decBuff, &encodeMesh);
	if (!obj_status.ok())
	{
		return -1;
	}

	draco::Encoder encoder;
	draco::EncoderBuffer buffer;

	encoder.SetAttributeQuantization(draco::GeometryAttribute::POSITION,
		25);
	encoder.SetAttributeQuantization(draco::GeometryAttribute::GENERIC,
		25);

	encoder.SetSpeedOptions(-1, -1);

	const draco::Status status = encoder.EncodeMeshToBuffer(encodeMesh, &buffer);
	if (!status.ok()) {
		return -2;
	}

	*data = new char[buffer.size()];

	const char* dataPoint = buffer.data();
	memcpy(*data, (void*)dataPoint, buffer.size());

	return buffer.size();
}


int EncodeMeshForUnity(char **data, DracoToUnityMesh **tmp_mesh)
{
	
	// 新建用于编码的网格
	draco::Mesh encodeMesh;


	DracoToUnityMesh *mesh = *tmp_mesh;
	//设置网格顶点数目,因为draco使用pointcloud所以直接按照面的数量乘3
	encodeMesh.set_num_points(mesh->num_vertices);
	//encodeMesh->SetNumFaces(mesh->num_faces);
	//创建顶点属性 
	GeometryAttribute pos;
	pos.Init(GeometryAttribute::POSITION, nullptr, 3, DT_FLOAT32, false,
		sizeof(float) * 3, 0);
	int posAttId = encodeMesh.AddAttribute(pos, true,
		mesh->num_vertices);
	

	//设置顶点的值
	draco::PointAttribute* posAtt = encodeMesh.attribute(posAttId);
	for (int i = 0; i < mesh->num_vertices; ++i)
	{
		float tmpVertex[3];
		tmpVertex[0] = mesh->position[i * 3];
		tmpVertex[1] = mesh->position[i * 3 + 1];
		tmpVertex[2] = mesh->position[i * 3 + 2];
		posAtt->SetAttributeValue(AttributeValueIndex(i), tmpVertex);
	}

	//设置顶点索引
	for (int i = 0; i < mesh->num_faces; ++i)
	{
		Mesh::Face face;
		face[0] = mesh->indices[i * 3];
		face[1] = mesh->indices[i * 3 + 1];
		face[2] = mesh->indices[i * 3 + 2];
		encodeMesh.SetFace(FaceIndex(i), face);
		//encodeMesh.AddFace(test);
	}

	//设置法线
	if (mesh->has_normal)
	{
		GeometryAttribute Nom;
		Nom.Init(GeometryAttribute::NORMAL, nullptr, 3, DT_FLOAT32, false,
			sizeof(float) * 3, 0);
		int norAttId = encodeMesh.AddAttribute(Nom, true,
			mesh->num_faces);


		//设置法线的值
		draco::PointAttribute* norAtt = encodeMesh.attribute(norAttId);
		for (int i = 0; i < mesh->num_faces; ++i)
		{
			float tmpNormal[3];
			tmpNormal[0] = mesh->normal[i * 3];
			tmpNormal[1] = mesh->normal[i * 3 + 1];
			tmpNormal[1] = mesh->normal[i * 3 + 2];
			norAtt->SetAttributeValue(AttributeValueIndex(i), tmpNormal);
		}
	}

	//设置UV
	if (mesh->has_texcoord)
	{
		GeometryAttribute UV;
		UV.Init(GeometryAttribute::TEX_COORD, nullptr, 2, DT_FLOAT32, false,
			sizeof(float) * 2, 0);
		int uvAttId = encodeMesh.AddAttribute(UV, true,
			mesh->num_vertices);


		//设置UV的值
		draco::PointAttribute* uvAtt = encodeMesh.attribute(uvAttId);
		for (int i = 0; i < mesh->num_vertices; ++i)
		{
			float tmpUV[2];
			tmpUV[0] = mesh->texcoord[i * 2];
			tmpUV[1] = mesh->texcoord[i * 2 + 1];
			uvAtt->SetAttributeValue(AttributeValueIndex(i), tmpUV);
		}
	}
	

	int posnum = encodeMesh.num_points();
	int facenum = encodeMesh.num_faces();
	int attnum = encodeMesh.num_attributes();

	draco::Encoder encoder;
	draco::EncoderBuffer buffer;

	encoder.SetAttributeQuantization(draco::GeometryAttribute::POSITION,
		25);
	encoder.SetAttributeQuantization(draco::GeometryAttribute::GENERIC,
		25);

	encoder.SetSpeedOptions(-1, -1);

	const draco::Status status = encoder.EncodeMeshToBuffer(encodeMesh, &buffer);
	if (!status.ok()) {
		printf("Failed to encode the mesh.\n");
		printf("%s\n", status.error_msg());
		return -1;
	}


	*data = new char[buffer.size()];

	const char* dataPoint = buffer.data();
	memcpy(*data, (void*)dataPoint, buffer.size());

	return buffer.size();
}

int DecodeMeshForUnity(char *data, unsigned int length,
                       DracoToUnityMesh **tmp_mesh) {
  draco::DecoderBuffer buffer;
  buffer.Init(data, length);
  auto type_statusor = draco::Decoder::GetEncodedGeometryType(&buffer);
  if (!type_statusor.ok()) {
    // TODO(draco-eng): Use enum instead.
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

  *tmp_mesh = new DracoToUnityMesh();
  DracoToUnityMesh *unity_mesh = *tmp_mesh;
  unity_mesh->num_faces = in_mesh->num_faces();
  unity_mesh->num_vertices = in_mesh->num_points();

  unity_mesh->indices = new int[in_mesh->num_faces() * 3];
  for (draco::FaceIndex face_id(0); face_id < in_mesh->num_faces(); ++face_id) {
    const Mesh::Face &face = in_mesh->face(draco::FaceIndex(face_id));
    memcpy(unity_mesh->indices + face_id.value() * 3,
           reinterpret_cast<const int *>(face.data()), sizeof(int) * 3);
  }

  // TODO(draco-eng): Add other attributes.
  unity_mesh->position = new float[in_mesh->num_points() * 3];
  const auto pos_att =
      in_mesh->GetNamedAttribute(draco::GeometryAttribute::POSITION);
  for (draco::PointIndex i(0); i < in_mesh->num_points(); ++i) {
    const draco::AttributeValueIndex val_index = pos_att->mapped_index(i);
    if (!pos_att->ConvertValue<float, 3>(
            val_index, unity_mesh->position + i.value() * 3)) {
      ReleaseUnityMesh(&unity_mesh);
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
        ReleaseUnityMesh(&unity_mesh);
        return -8;
      }
    }
  }
  // Get color attributes.
  const auto color_att =
      in_mesh->GetNamedAttribute(draco::GeometryAttribute::COLOR);
  if (color_att != nullptr) {
    unity_mesh->color = new float[in_mesh->num_points() * 4];
    unity_mesh->has_color = true;
    for (draco::PointIndex i(0); i < in_mesh->num_points(); ++i) {
      const draco::AttributeValueIndex val_index = color_att->mapped_index(i);
      if (!color_att->ConvertValue<float, 4>(
              val_index, unity_mesh->color + i.value() * 4)) {
        ReleaseUnityMesh(&unity_mesh);
        return -8;
      }
      if (color_att->num_components() < 4) {
        // If the alpha component wasn't set in the input data we should set
        // it to an opaque value.
        unity_mesh->color[i.value() * 4 + 3] = 1.f;
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
      if (!texcoord_att->ConvertValue<float, 2>(
              val_index, unity_mesh->texcoord + i.value() * 2)) {
        ReleaseUnityMesh(&unity_mesh);
        return -8;
      }
    }
  }

  //获得子网格
  const GeometryMetadata *sub_mesh_mapdata =
	  in_mesh->metadata();
  if (sub_mesh_mapdata != nullptr)
  {
	  int submeshCount = 0;
	  if (sub_mesh_mapdata->GetEntryInt("submeshCount", &submeshCount))
	  {
		  unity_mesh->num_submesh = submeshCount;
		  unity_mesh->submesh = new int*[submeshCount];
		  int submeshIndex = 0;
		  for (int submeshIndex = 0; submeshIndex < submeshCount; submeshIndex++)
		  {
			  std::vector<int>* submeshFaceArray = nullptr;
			  if (sub_mesh_mapdata->GetEntryIntArray(std::to_string(submeshIndex), submeshFaceArray))
			  {
				  unity_mesh->submesh[submeshIndex] = new int[submeshFaceArray->size() + 1];
				  unity_mesh->submesh[submeshIndex][0] = submeshFaceArray->size();
				  for (int submeshFaceIndex = 0; submeshFaceIndex < unity_mesh->submesh[submeshIndex][0]; submeshFaceIndex++)
				  {
					  unity_mesh->submesh[submeshIndex][submeshFaceIndex + 1] = submeshFaceArray->at(submeshFaceIndex);
				  }
			  }
		  }
	  }
  }

  return in_mesh->num_faces();
}

void FreeDracoMesh(char **data)
{
	if (*data == nullptr)
	{
		return;
	}
	delete[] * data;
	data = nullptr;
}

}  // namespace draco

#endif  // BUILD_UNITY_PLUGIN
