// Copyright 2016 The Draco Authors.
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
#ifndef DRACO_MESH_MESH_H_
#define DRACO_MESH_MESH_H_

#include <memory>

#include "draco/attributes/geometry_indices.h"
#include "draco/core/hash_utils.h"
#include "draco/core/macros.h"
#include "draco/core/status.h"
#include "draco/draco_features.h"
#ifdef DRACO_TRANSCODER_SUPPORTED
#include "draco/compression/draco_compression_options.h"
#include "draco/material/material_library.h"
#endif
#include "draco/point_cloud/point_cloud.h"

namespace draco {

// List of different variants of mesh attributes.
enum MeshAttributeElementType {
  // All corners attached to a vertex share the same attribute value. A typical
  // example are the vertex positions and often vertex colors.
  MESH_VERTEX_ATTRIBUTE = 0,
  // The most general attribute where every corner of the mesh can have a
  // different attribute value. Often used for texture coordinates or normals.
  MESH_CORNER_ATTRIBUTE,
  // All corners of a single face share the same value.
  MESH_FACE_ATTRIBUTE
};

// Mesh class can be used to represent general triangular meshes. Internally,
// Mesh is just an extended PointCloud with extra connectivity data that defines
// what points are connected together in triangles.
class Mesh : public PointCloud {
 public:
  typedef std::array<PointIndex, 3> Face;

  Mesh();

#ifdef DRACO_TRANSCODER_SUPPORTED
  // Copies all data from the |src| mesh.
  void Copy(const Mesh &src);
#endif

  void AddFace(const Face &face) { faces_.push_back(face); }

  void SetFace(FaceIndex face_id, const Face &face) {
    if (face_id >= static_cast<uint32_t>(faces_.size())) {
      faces_.resize(face_id.value() + 1, Face());
    }
    faces_[face_id] = face;
  }

  // Sets the total number of faces. Creates new empty faces or deletes
  // existing ones if necessary.
  void SetNumFaces(size_t num_faces) { faces_.resize(num_faces, Face()); }

  FaceIndex::ValueType num_faces() const {
    return static_cast<uint32_t>(faces_.size());
  }
  const Face &face(FaceIndex face_id) const {
    DRACO_DCHECK_LE(0, face_id.value());
    DRACO_DCHECK_LT(face_id.value(), static_cast<int>(faces_.size()));
    return faces_[face_id];
  }

  void SetAttribute(int att_id, std::unique_ptr<PointAttribute> pa) override {
    PointCloud::SetAttribute(att_id, std::move(pa));
    if (static_cast<int>(attribute_data_.size()) <= att_id) {
      attribute_data_.resize(att_id + 1);
    }
  }

  void DeleteAttribute(int att_id) override {
    PointCloud::DeleteAttribute(att_id);
    if (att_id >= 0 && att_id < static_cast<int>(attribute_data_.size())) {
      attribute_data_.erase(attribute_data_.begin() + att_id);
    }
  }

#ifdef DRACO_TRANSCODER_SUPPORTED
  // Adds a point attribute |att| to the mesh and returns the index of the
  // newly inserted attribute. Attribute connectivity data is specified in
  // |corner_to_value| array that contains mapping between face corners and
  // attribute value indices.
  // The purpose of this function is to allow users to add attributes with
  // arbitrary connectivity to an existing mesh. New points will be
  // automatically created if needed.
  int32_t AddAttributeWithConnectivity(
      std::unique_ptr<PointAttribute> att,
      const IndexTypeVector<CornerIndex, AttributeValueIndex> &corner_to_value);

  // Adds a point attribute |att| to the mesh and returns the index of the
  // newly inserted attribute. The inserted attribute must have the same
  // connectivity as the position attribute of the mesh (that is, the attribute
  // values are defined per-vertex). Each attribute value entry in |att|
  // corresponds to the corresponding attribute value entry in the position
  // attribute (AttributeValueIndex in both attributes refer to the same
  // spatial vertex).
  // Returns -1 in case of error.
  int32_t AddPerVertexAttribute(std::unique_ptr<PointAttribute> att);

  // Removes points that are not mapped to any face of the mesh. All attribute
  // values are going to be removed as well.
  void RemoveIsolatedPoints();
#endif

  MeshAttributeElementType GetAttributeElementType(int att_id) const {
    return attribute_data_[att_id].element_type;
  }

  void SetAttributeElementType(int att_id, MeshAttributeElementType et) {
    attribute_data_[att_id].element_type = et;
  }

  // Returns the point id of for a corner |ci|.
  inline PointIndex CornerToPointId(int ci) const {
    if (ci < 0 || static_cast<uint32_t>(ci) == kInvalidCornerIndex.value()) {
      return kInvalidPointIndex;
    }
    return this->face(FaceIndex(ci / 3))[ci % 3];
  }

  // Returns the point id of a corner |ci|.
  inline PointIndex CornerToPointId(CornerIndex ci) const {
    return this->CornerToPointId(ci.value());
  }

  struct AttributeData {
    AttributeData() : element_type(MESH_CORNER_ATTRIBUTE) {}
    MeshAttributeElementType element_type;
  };

#ifdef DRACO_TRANSCODER_SUPPORTED
  void SetName(const std::string &name) { name_ = name; }
  const std::string &GetName() const { return name_; }
  const MaterialLibrary &GetMaterialLibrary() const {
    return material_library_;
  }
  MaterialLibrary &GetMaterialLibrary() { return material_library_; }

  // Removes all materials that are not referenced by any face of the mesh.
  void RemoveUnusedMaterials();

  // Enables or disables Draco geometry compression for this mesh.
  void SetCompressionEnabled(bool enabled) { compression_enabled_ = enabled; }
  bool IsCompressionEnabled() const { return compression_enabled_; }

  // Sets |options| that configure Draco geometry compression. This does not
  // enable or disable compression.
  void SetCompressionOptions(const DracoCompressionOptions &options) {
    compression_options_ = options;
  }
  const DracoCompressionOptions &GetCompressionOptions() const {
    return compression_options_;
  }
  DracoCompressionOptions &GetCompressionOptions() {
    return compression_options_;
  }
#endif

 protected:
#ifdef DRACO_ATTRIBUTE_INDICES_DEDUPLICATION_SUPPORTED
  // Extends the point deduplication to face corners. This method is called from
  // the PointCloud::DeduplicatePointIds() and it remaps all point ids stored in
  // |faces_| to the new deduplicated point ids using the map |id_map|.
  void ApplyPointIdDeduplication(
      const IndexTypeVector<PointIndex, PointIndex> &id_map,
      const std::vector<PointIndex> &unique_point_ids) override;
#endif

  // Exposes |faces_|. Use |faces_| at your own risk. DO NOT store the
  // reference: the |faces_| object is destroyed with the mesh.
  IndexTypeVector<FaceIndex, Face> &faces() { return faces_; }

 private:
  // Mesh specific per-attribute data.
  std::vector<AttributeData> attribute_data_;

  // Vertex indices valid for all attributes. Each attribute has its own map
  // that converts vertex indices into attribute indices.
  IndexTypeVector<FaceIndex, Face> faces_;

#ifdef DRACO_TRANSCODER_SUPPORTED
  // Mesh name.
  std::string name_;

  // Materials applied to to this mesh.
  MaterialLibrary material_library_;

  // Compression options for this mesh.
  // TODO(vytyaz): Store encoded bitstream that this mesh compresses into.
  bool compression_enabled_;
  DracoCompressionOptions compression_options_;
#endif
  friend struct MeshHasher;
};

// Functor for computing a hash from data stored within a mesh.
// Note that this can be quite slow. Two meshes will have the same hash only
// when they have exactly the same connectivity and attribute values.
struct MeshHasher {
  size_t operator()(const Mesh &mesh) const {
    PointCloudHasher pc_hasher;
    size_t hash = pc_hasher(mesh);
    // Hash faces.
    for (FaceIndex i(0); i < static_cast<uint32_t>(mesh.faces_.size()); ++i) {
      for (int j = 0; j < 3; ++j) {
        hash = HashCombine(mesh.faces_[i][j].value(), hash);
      }
    }
    return hash;
  }
};

}  // namespace draco

#endif  // DRACO_MESH_MESH_H_
