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
#include "draco/mesh/mesh_splitter.h"

#ifdef DRACO_TRANSCODER_SUPPORTED
#include <utility>

namespace draco {

MeshSplitter::MeshSplitter() : preserve_materials_(false) {}

StatusOr<MeshSplitter::MeshVector> MeshSplitter::SplitMesh(
    const Mesh &mesh, uint32_t split_attribute_id) {
  if (mesh.num_attributes() <= split_attribute_id) {
    return Status(Status::DRACO_ERROR, "Invalid attribute id.");
  }

  const PointAttribute *const split_attribute =
      mesh.attribute(split_attribute_id);

  // Preserve the split attribute only if it is the material attribute and the
  // |preserve_materials_| flag is set. Othwerwise the split attribute will get
  // discarded.
  // TODO(ostava): We may revisit this later and add an option to always
  // preserve the split attribute.
  const bool preserve_split_attribute =
      preserve_materials_ &&
      split_attribute->attribute_type() == GeometryAttribute::MATERIAL;

  const int num_out_meshes = split_attribute->size();
  WorkData work_data;
  work_data.num_sub_mesh_faces.resize(num_out_meshes, 0);

  // Verify that the attribute values are defined "per-face", i.e., all points
  // on a face are always mapped to the same attribute value.
  for (FaceIndex fi(0); fi < mesh.num_faces(); ++fi) {
    const auto face = mesh.face(fi);
    const AttributeValueIndex avi = split_attribute->mapped_index(face[0]);
    for (int c = 1; c < 3; ++c) {
      if (split_attribute->mapped_index(face[c]) != avi) {
        return Status(Status::DRACO_ERROR,
                      "Attribute values not consistent on a face.");
      }
    }
    work_data.num_sub_mesh_faces[avi.value()] += 1;
  }

  // Create the sub-meshes.
  work_data.mesh_builders.resize(num_out_meshes);
  // Map between attribute ids of the input and output meshes.
  work_data.att_id_map.resize(mesh.num_attributes(), -1);
  const int ignored_att_id =
      (!preserve_split_attribute ? split_attribute_id : -1);
  for (int mi = 0; mi < num_out_meshes; ++mi) {
    if (work_data.num_sub_mesh_faces[mi] == 0) {
      continue;  // Empty mesh, don't initialize it.
    }

    const int num_faces = work_data.num_sub_mesh_faces[mi];
    InitializeMeshBuilder(mi, num_faces, mesh, ignored_att_id, &work_data);

    // Reset the face counter for the sub-mesh. It will be used to keep track of
    // number of faces added to the sub-mesh.
    work_data.num_sub_mesh_faces[mi] = 0;
  }

  // Go over all faces of the input mesh and add them to the appropriate
  // sub-mesh.
  for (FaceIndex fi(0); fi < mesh.num_faces(); ++fi) {
    const auto face = mesh.face(fi);
    const int sub_mesh_id = split_attribute->mapped_index(face[0]).value();
    const FaceIndex target_fi(work_data.num_sub_mesh_faces[sub_mesh_id]++);
    AddFaceToMeshBuilder(sub_mesh_id, fi, target_fi, mesh, &work_data);
  }

  return FinalizeMeshes(mesh, &work_data);
}

StatusOr<MeshSplitter::MeshVector> MeshSplitter::SplitMeshToComponents(
    const Mesh &mesh, const MeshConnectedComponents &connected_components) {
  // Create the sub-meshes.
  const int num_out_meshes = connected_components.NumConnectedComponents();
  WorkData work_data;
  work_data.mesh_builders.resize(num_out_meshes);
  work_data.num_sub_mesh_faces.resize(num_out_meshes, 0);
  work_data.att_id_map.resize(mesh.num_attributes(), -1);
  for (int mi = 0; mi < num_out_meshes; ++mi) {
    const int num_faces = connected_components.NumConnectedComponentFaces(mi);
    work_data.num_sub_mesh_faces[mi] = num_faces;
    InitializeMeshBuilder(mi, num_faces, mesh, -1, &work_data);
  }

  // Go over all faces of the input mesh and add them to the appropriate
  // sub-mesh.
  for (int mi = 0; mi < num_out_meshes; ++mi) {
    for (int cfi = 0; cfi < connected_components.NumConnectedComponentFaces(mi);
         ++cfi) {
      const FaceIndex fi(
          connected_components.GetConnectedComponent(mi).faces[cfi]);
      const FaceIndex target_fi(cfi);
      AddFaceToMeshBuilder(mi, fi, target_fi, mesh, &work_data);
    }
  }
  return FinalizeMeshes(mesh, &work_data);
}

void MeshSplitter::InitializeMeshBuilder(int mb_index, int num_faces,
                                         const Mesh &mesh,
                                         int ignored_attribute_id,
                                         WorkData *work_data) const {
  work_data->mesh_builders[mb_index].Start(num_faces);
  work_data->mesh_builders[mb_index].SetName(mesh.GetName());

  // Add all attributes.
  for (int ai = 0; ai < mesh.num_attributes(); ++ai) {
    if (ai == ignored_attribute_id) {
      continue;
    }
    const GeometryAttribute *const src_att = mesh.attribute(ai);
    work_data->att_id_map[ai] = work_data->mesh_builders[mb_index].AddAttribute(
        src_att->attribute_type(), src_att->num_components(),
        src_att->data_type());
  }
}

void MeshSplitter::AddFaceToMeshBuilder(int mb_index, FaceIndex source_fi,
                                        FaceIndex target_fi, const Mesh &mesh,
                                        WorkData *work_data) const {
  const auto face = mesh.face(source_fi);
  for (int ai = 0; ai < mesh.num_attributes(); ++ai) {
    const PointAttribute *const src_att = mesh.attribute(ai);
    const int target_att_id = work_data->att_id_map[ai];
    if (target_att_id == -1) {
      continue;
    }
    work_data->mesh_builders[mb_index].SetAttributeValuesForFace(
        target_att_id, target_fi, src_att->GetAddressOfMappedIndex(face[0]),
        src_att->GetAddressOfMappedIndex(face[1]),
        src_att->GetAddressOfMappedIndex(face[2]));
  }
}

StatusOr<MeshSplitter::MeshVector> MeshSplitter::FinalizeMeshes(
    const Mesh &mesh, WorkData *work_data) const {
  // Finalize meshes.
  const int num_out_meshes = work_data->mesh_builders.size();
  MeshVector out_meshes(num_out_meshes);
  for (int mi = 0; mi < num_out_meshes; ++mi) {
    if (work_data->num_sub_mesh_faces[mi] == 0) {
      continue;
    }
    out_meshes[mi] = work_data->mesh_builders[mi].Finalize();
    if (out_meshes[mi] == nullptr) {
      continue;
    }
    if (preserve_materials_) {
      out_meshes[mi]->GetMaterialLibrary().Copy(mesh.GetMaterialLibrary());
      out_meshes[mi]->RemoveUnusedMaterials();
    }

    // Copy metadata of the original mesh to the output meshes.
    if (mesh.GetMetadata() != nullptr) {
      const Metadata &metadata = *mesh.GetMetadata();
      out_meshes[mi]->AddMetadata(
          std::unique_ptr<GeometryMetadata>(new GeometryMetadata(metadata)));
    }

    // Copy over attribute unique ids.
    for (int att_id = 0; att_id < mesh.num_attributes(); ++att_id) {
      const int mapped_att_id = work_data->att_id_map[att_id];
      if (mapped_att_id == -1) {
        continue;
      }
      const PointAttribute *const src_att = mesh.attribute(att_id);
      PointAttribute *const dst_att = out_meshes[mi]->attribute(mapped_att_id);
      dst_att->set_unique_id(src_att->unique_id());
    }

    // Copy compression settings of the original mesh to the output meshes.
    out_meshes[mi]->SetCompressionEnabled(mesh.IsCompressionEnabled());
    out_meshes[mi]->SetCompressionOptions(mesh.GetCompressionOptions());
  }
  return std::move(out_meshes);
}

}  // namespace draco
#endif  // DRACO_TRANSCODER_SUPPORTED
