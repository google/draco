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
#include "compression/mesh/mesh_edgebreaker_encoder.h"

#include "compression/mesh/mesh_edgebreaker_encoder_impl.h"
#include "compression/mesh/mesh_edgebreaker_traversal_predictive_encoder.h"
#include "compression/mesh/mesh_edgebreaker_traversal_valence_encoder.h"

namespace draco {

MeshEdgeBreakerEncoder::MeshEdgeBreakerEncoder() {}

bool MeshEdgeBreakerEncoder::InitializeEncoder() {
  const bool is_standard_edgebreaker_avaialable =
      options()->IsFeatureSupported(features::kEdgebreaker);
  const bool is_predictive_edgebreaker_avaialable =
      options()->IsFeatureSupported(features::kPredictiveEdgebreaker);

  impl_ = nullptr;
  // For tiny meshes it's usually better to use the basic edgebreaker as the
  // overhead of the predictive one may turn out to be too big.
  // TODO(ostava): For now we have a set limit for forcing the basic edgebreaker
  // based on the number of faces, but a more complex heuristic may be used if
  // needed.
  const bool is_tiny_mesh = mesh()->num_faces() < 1000;

  if (is_standard_edgebreaker_avaialable &&
      (options()->GetSpeed() >= 5 || !is_predictive_edgebreaker_avaialable ||
       is_tiny_mesh)) {
    buffer()->Encode(static_cast<uint8_t>(0));
    impl_ = std::unique_ptr<MeshEdgeBreakerEncoderImplInterface>(
        new MeshEdgeBreakerEncoderImpl<MeshEdgeBreakerTraversalEncoder>());
  } else if (is_predictive_edgebreaker_avaialable) {
    buffer()->Encode(static_cast<uint8_t>(2));
    impl_ = std::unique_ptr<MeshEdgeBreakerEncoderImplInterface>(
        new MeshEdgeBreakerEncoderImpl<
            MeshEdgeBreakerTraversalValenceEncoder>());
  }
  if (!impl_)
    return false;
  if (!impl_->Init(this))
    return false;
  return true;
}

bool MeshEdgeBreakerEncoder::GenerateAttributesEncoder(int32_t att_id) {
  if (!impl_->GenerateAttributesEncoder(att_id))
    return false;
  return true;
}

bool MeshEdgeBreakerEncoder::EncodeAttributesEncoderIdentifier(
    int32_t att_encoder_id) {
  if (!impl_->EncodeAttributesEncoderIdentifier(att_encoder_id))
    return false;
  return true;
}

bool MeshEdgeBreakerEncoder::EncodeConnectivity() {
  return impl_->EncodeConnectivity();
}

}  // namespace draco
