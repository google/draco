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

#include "draco/io/tiny_gltf_utils.h"

#ifdef DRACO_TRANSCODER_SUPPORTED
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

#include "Eigen/Dense"
#include "draco/animation/animation.h"
#include "draco/animation/node_animation_data.h"
#include "draco/core/status.h"
#include "draco/core/status_or.h"
#include "draco/core/vector_d.h"
#include "tiny_gltf.h"

namespace draco {

int TinyGltfUtils::GetNumComponentsForType(int type) {
  switch (type) {
    case TINYGLTF_TYPE_SCALAR:
      return 1;
    case TINYGLTF_TYPE_VEC2:
      return 2;
    case TINYGLTF_TYPE_VEC3:
      return 3;
    case TINYGLTF_TYPE_VEC4:
      return 4;
    case TINYGLTF_TYPE_MAT2:
      return 4;
    case TINYGLTF_TYPE_MAT3:
      return 9;
    case TINYGLTF_TYPE_MAT4:
      return 16;
    default:
      return 0;
  }
}

int TinyGltfUtils::GetComponentSize(int component_type) {
  switch (component_type) {
    case TINYGLTF_COMPONENT_TYPE_BYTE:
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
      return 1;
    case TINYGLTF_COMPONENT_TYPE_SHORT:
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
      return 2;
    case TINYGLTF_COMPONENT_TYPE_INT:
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
    case TINYGLTF_COMPONENT_TYPE_FLOAT:
      return 4;
    case TINYGLTF_COMPONENT_TYPE_DOUBLE:
      return 8;
    default:
      return 0;
  }
}

StatusOr<int> TinyGltfUtils::GetRequiredBytes(const tinygltf::Accessor &accessor) {
  const int num_components = GetNumComponentsForType(accessor.type);
  const int component_size = GetComponentSize(accessor.componentType);
  if (num_components == 0 || component_size == 0) {
    return Status(Status::DRACO_ERROR, "Invalid accessor.");
  }
  return num_components * component_size * accessor.count;
}

Status TinyGltfUtils::CheckAccessorBounds(const tinygltf::Model &model,
                                           const tinygltf::Accessor &accessor) {
  if (accessor.bufferView < 0 ||
      accessor.bufferView >= model.bufferViews.size()) {
    return Status(Status::DRACO_ERROR, "BufferView index out of bounds.");
  }
  const tinygltf::BufferView &buffer_view =
      model.bufferViews[accessor.bufferView];
  if (buffer_view.buffer < 0 || buffer_view.buffer >= model.buffers.size()) {
    return Status(Status::DRACO_ERROR, "Buffer index out of bounds.");
  }
  const tinygltf::Buffer &buffer = model.buffers[buffer_view.buffer];
  if (buffer_view.byteOffset > buffer.data.size() ||
      buffer_view.byteLength > buffer.data.size() - buffer_view.byteOffset) {
    return Status(Status::DRACO_ERROR, "BufferView out of bounds.");
  }
  if (accessor.byteOffset > buffer_view.byteLength) {
    return Status(Status::DRACO_ERROR, "Accessor out of bounds.");
  }
  DRACO_ASSIGN_OR_RETURN(const int required_bytes, GetRequiredBytes(accessor));
  const size_t accessor_byte_stride =
      accessor.ByteStride(buffer_view) == 0 ? required_bytes / accessor.count
                                            : accessor.ByteStride(buffer_view);
  if (accessor.count == 0) {
    return Status::Ok();
  }
  if (accessor_byte_stride > 0 &&
      (accessor.count - 1) * accessor_byte_stride + required_bytes / accessor.count >
          buffer_view.byteLength - accessor.byteOffset) {
    return Status(Status::DRACO_ERROR, "Accessor out of bounds.");
  }
  if (required_bytes >
      buffer.data.size() - buffer_view.byteOffset - accessor.byteOffset) {
    return Status(Status::DRACO_ERROR, "Accessor out of bounds.");
  }
  if (accessor.sparse.count > 0) {
    const size_t required_sparse_indices_bytes =
        GetRequiredBytes(accessor.sparse.indices);
    if (required_sparse_indices_bytes >
        buffer.data.size() - buffer_view.byteOffset - accessor.byteOffset) {
      return Status(Status::DRACO_ERROR, "Sparse accessor out of bounds.");
    }
    const size_t required_sparse_values_bytes =
        GetRequiredBytes(accessor.sparse.values);
    if (required_sparse_values_bytes >
        buffer.data.size() - buffer_view.byteOffset - accessor.byteOffset) {
      return Status(Status::DRACO_ERROR, "Sparse accessor out of bounds.");
    }
  }
  return Status::Ok();
}

Status TinyGltfUtils::AddAccessorToAnimationData(
    const tinygltf::Model &model, const tinygltf::Accessor &accessor,
    AnimationData::Sampler::ValueVector *values,
    AnimationData::Sampler::Interpolation interpolation) {
  // Add all the values to the animation data.
  DRACO_RETURN_IF_ERROR(CheckAccessorBounds(model, accessor));
  const tinygltf::BufferView &buffer_view =
      model.bufferViews[accessor.bufferView];
  const tinygltf::Buffer &buffer = model.buffers[buffer_view.buffer];
  const size_t byte_stride = accessor.ByteStride(buffer_view);
  const uint8_t *const data = buffer.data.data() + buffer_view.byteOffset +
                              accessor.byteOffset;
  const int component_size = GetComponentSize(accessor.componentType);
  const int num_components = GetNumComponentsForType(accessor.type);
  values->reserve(accessor.count);
  for (int i = 0; i < accessor.count; ++i) {
    const uint8_t *const value_data = data + i * byte_stride;
    Eigen::VectorXd value = Eigen::VectorXd::Zero(num_components);
    for (int j = 0; j < num_components; ++j) {
      const uint8_t *const component_data = value_data + j * component_size;
      switch (accessor.componentType) {
        case TINYGLTF_COMPONENT_TYPE_BYTE:
          value[j] = *reinterpret_cast<const int8_t *>(component_data);
          break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
          value[j] = *reinterpret_cast<const uint8_t *>(component_data);
          break;
        case TINYGLTF_COMPONENT_TYPE_SHORT:
          value[j] = *reinterpret_cast<const int16_t *>(component_data);
          break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
          value[j] = *reinterpret_cast<const uint16_t *>(component_data);
          break;
        case TINYGLTF_COMPONENT_TYPE_INT:
          value[j] = *reinterpret_cast<const int32_t *>(component_data);
          break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
          value[j] = *reinterpret_cast<const uint32_t *>(component_data);
          break;
        case TINYGLTF_COMPONENT_TYPE_FLOAT:
          value[j] = *reinterpret_cast<const float *>(component_data);
          break;
        case TINYGLTF_COMPONENT_TYPE_DOUBLE:
          value[j] = *reinterpret_cast<const double *>(component_data);
          break;
        default:
          return Status(Status::DRACO_ERROR, "Invalid component type.");
      }
    }
    values->push_back(value);
  }
  return Status::Ok();
}

Status TinyGltfUtils::AddSamplerToAnimation(
    const tinygltf::Model &model, const tinygltf::Animation &input_animation,
    const tinygltf::AnimationChannel &channel, Scene *target_scene,
    Scene *output_scene) {
  const tinygltf::AnimationSampler &sampler =
      input_animation.samplers[channel.sampler];
  return AddSamplerToAnimation(model, sampler, channel, target_scene,
                               output_scene);
}

Status TinyGltfUtils::AddSamplerToAnimation(
    const tinygltf::Model &model, const tinygltf::AnimationSampler &sampler,
    const tinygltf::AnimationChannel &channel, Scene *target_scene,
    Scene *output_scene) {
  // Add the sampler data to the animation.
  if (channel.target_node >= 0 &&
      channel.target_node < model.nodes.size()) {
    const int node_id = channel.target_node;
    // Get the target node from the target scene.
    SceneNodeIndex target_node_index = target_scene->GetNodeIndex(node_id);
    if (target_node_index == -1) {
      // Target node not found. It may have been removed during processing.
      return Status::Ok();
    }
    // Get the animation data for the target node.
    NodeAnimationData *node_animation_data =
        output_scene->GetNodeAnimationData(target_node_index);
    if (node_animation_data == nullptr) {
      // Node animation data not found. It may have been removed during
      // processing.
      return Status::Ok();
    }
    // Get the sampler id.
    const int sampler_id = node_animation_data->AddSampler();
    AnimationData::Sampler *output_sampler =
        node_animation_data->GetSampler(sampler_id);
    if (output_sampler == nullptr) {
      return Status(Status::DRACO_ERROR, "Failed to add sampler.");
    }
    if (sampler.input < 0 ||
        sampler.input >= static_cast<int>(model.accessors.size())) {
      return Status(Status::DRACO_ERROR, "Input accessor index out of bounds.");
    }
    const tinygltf::Accessor &input_accessor = model.accessors[sampler.input];
    AnimationData::Sampler::Interpolation interpolation;
    if (sampler.interpolation == "LINEAR") {
      interpolation = AnimationData::Sampler::Interpolation::kLinear;
    } else if (sampler.interpolation == "STEP") {
      interpolation = AnimationData::Sampler::Interpolation::kStep;
    } else if (sampler.interpolation == "CUBICSPLINE") {
      interpolation = AnimationData::Sampler::Interpolation::kCubicSpline;
    } else {
      return Status(Status::DRACO_ERROR, "Invalid interpolation type.");
    }
    DRACO_RETURN_IF_ERROR(AddAccessorToAnimationData(
        model, input_accessor, output_sampler->GetSamplerTimes(sampler_id),
        AnimationData::Sampler::Interpolation::kLinear));
    if (sampler.output < 0 ||
        sampler.output >= static_cast<int>(model.accessors.size())) {
      return Status(Status::DRACO_ERROR, "Output accessor index out of bounds.");
    }
    const tinygltf::Accessor &output_accessor = model.accessors[sampler.output];
    DRACO_RETURN_IF_ERROR(AddAccessorToAnimationData(
        model, output_accessor, output_sampler->GetSamplerValues(sampler_id),
        interpolation));
    return Status::Ok();
  }
  return Status(Status::DRACO_ERROR, "Invalid target node.");
}

Status TinyGltfUtils::AddChannelToAnimation(
    const tinygltf::Model &model, const tinygltf::Animation &input_animation,
    const tinygltf::AnimationChannel &channel, Scene *target_scene,
    Scene *output_scene) {
  // Add the channel data to the animation.
  if (channel.sampler < 0 ||
      channel.sampler >=
          static_cast<int>(input_animation.samplers.size())) {
    return Status(Status::DRACO_ERROR, "Sampler index out of bounds.");
  }
  const tinygltf::AnimationSampler &sampler =
      input_animation.samplers[channel.sampler];
  // Get the target node.
  if (channel.target_node >= 0 &&
      channel.target_node < model.nodes.size()) {
    const int node_id = channel.target_node;
    // Get the target node from the target scene.
    SceneNodeIndex target_node_index = target_scene->GetNodeIndex(node_id);
    if (target_node_index == -1) {
      // Target node not found. It may have been removed during processing.
      return Status::Ok();
    }
    // Get the animation data for the target node.
    NodeAnimationData *node_animation_data =
        output_scene->GetNodeAnimationData(target_node_index);
    if (node_animation_data == nullptr) {
      // Node animation data not found. It may have been removed during
      // processing.
      return Status::Ok();
    }
    // Get the sampler id.
    const int sampler_id = node_animation_data->AddSampler();
    AnimationData::Sampler *output_sampler =
        node_animation_data->GetSampler(sampler_id);
    if (output_sampler == nullptr) {
      return Status(Status::DRACO_ERROR, "Failed to add sampler.");
    }
    // Add the sampler data to the animation.
    DRACO_RETURN_IF_ERROR(AddSamplerToAnimation(model, sampler, channel,
                                                target_scene, output_scene));
    return Status::Ok();
  }
  return Status(Status::DRACO_ERROR, "Invalid target node.");
}

Status TinyGltfUtils::AddAnimationsToScene(const tinygltf::Model &model,
                                           Scene *target_scene,
                                           Scene *output_scene) {
  // Add all the animations to the scene.
  for (const auto &input_animation : model.animations) {
    for (const auto &channel : input_animation.channels) {
      DRACO_RETURN_IF_ERROR(AddChannelToAnimation(model, input_animation,
                                                  channel, target_scene,
                                                  output_scene));
    }
  }
  return Status::Ok();
}

}  // namespace draco

#endif  // DRACO_TRANSCODER_SUPPORTED
