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
#include "draco/mesh/triangle_soup_mesh_builder.h"

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

  // Verify that the attribute values are defined "per-face", i.e., all points
  // on a face are always mapped to the same attribute value.
  std::vector<int> num_sub_mesh_faces(num_out_meshes, 0);
  for (FaceIndex fi(0); fi < mesh.num_faces(); ++fi) {
    const auto face = mesh.face(fi);
    const AttributeValueIndex avi = split_attribute->mapped_index(face[0]);
    for (int c = 1; c < 3; ++c) {
      if (split_attribute->mapped_index(face[c]) != avi) {
        return Status(Status::DRACO_ERROR,
                      "Attribute values not consistent on a face.");
      }
    }
    num_sub_mesh_faces[avi.value()] += 1;
  }

  // Create the sub-meshes.
  std::vector<TriangleSoupMeshBuilder> mesh_builders(num_out_meshes);
  // Map between attribute ids of the input and output meshes.
  std::vector<int> att_id_map(mesh.num_attributes(), -1);
  for (int mi = 0; mi < num_out_meshes; ++mi) {
    if (num_sub_mesh_faces[mi] == 0) {
      continue;  // Empty mesh, don't initialize it.
    }

    const int num_faces = num_sub_mesh_faces[mi];
    mesh_builders[mi].Start(num_faces);
    mesh_builders[mi].SetName(mesh.GetName());

    // Add all attributes.
    for (int ai = 0; ai < mesh.num_attributes(); ++ai) {
      if (ai == split_attribute_id && !preserve_split_attribute) {
        continue;
      }
      const GeometryAttribute *const src_att = mesh.attribute(ai);
      att_id_map[ai] = mesh_builders[mi].AddAttribute(src_att->attribute_type(),
                                                      src_att->num_components(),
                                                      src_att->data_type());
    }

    // Reset the face counter for the sub-mesh. It will be used to keep track of
    // number of faces added to the sub-mesh.
    num_sub_mesh_faces[mi] = 0;
  }

  // Go over all faces of the input mesh and add them to the appropriate
  // sub-mesh.
  for (FaceIndex fi(0); fi < mesh.num_faces(); ++fi) {
    const auto face = mesh.face(fi);
    const int sub_mesh_id = split_attribute->mapped_index(face[0]).value();
    const FaceIndex target_face(num_sub_mesh_faces[sub_mesh_id]++);
    for (int ai = 0; ai < mesh.num_attributes(); ++ai) {
      if (ai == split_attribute_id && !preserve_split_attribute) {
        continue;
      }
      const PointAttribute *const src_att = mesh.attribute(ai);
      const int target_att_id = att_id_map[ai];
      mesh_builders[sub_mesh_id].SetAttributeValuesForFace(
          target_att_id, target_face, src_att->GetAddressOfMappedIndex(face[0]),
          src_att->GetAddressOfMappedIndex(face[1]),
          src_att->GetAddressOfMappedIndex(face[2]));
    }
  }

  // Finalize meshes.
  MeshVector out_meshes(num_out_meshes);
  for (int mi = 0; mi < num_out_meshes; ++mi) {
    if (num_sub_mesh_faces[mi] == 0) {
      continue;  // Empty mesh, don't create it.
    }
    out_meshes[mi] = mesh_builders[mi].Finalize();
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
      const int mapped_att_id = att_id_map[att_id];
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
