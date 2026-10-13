// Copyright 2024 The Draco Authors.
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
#ifndef DRACO_SCENE_AUDIO_H_
#define DRACO_SCENE_AUDIO_H_

#include "draco/draco_features.h"

#ifdef DRACO_TRANSCODER_SUPPORTED
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "draco/core/constants.h"

namespace draco {

// Describes scene audio according to the ADBE_audio_emitter extension. This is
// a snapshot of the KHR_audio_emitter extension that is under active
// development by Khrnos as of May 2024.
struct Audio {
  struct Emitter {
    struct Positional {
      float cone_inner_angle = 2 * DRACO_PI;
      float cone_outer_angle = 2 * DRACO_PI;
      float cone_outer_gain = 0.0f;
      std::string distance_model = "inverse";
      float max_distance = 0.0f;
      float ref_distance = 1.0f;
      float rolloff_factor = 1.0f;
    };
    std::string name;
    std::string type;
    float gain = 1.0f;
    std::vector<int> sources;
    std::optional<Positional> positional;
  };
  struct Source {
    std::string name;
    float gain = 1.0f;
    bool loop = false;
    bool auto_play = false;
    int audio = 0;
  };
  struct Data {
    std::string uri;
    std::string mime_type;
    std::vector<uint8_t> data;
  };
  std::vector<Emitter> emitters;
  std::vector<Source> sources;
  std::vector<Data> data;
};

}  // namespace draco

#endif  // DRACO_TRANSCODER_SUPPORTED
#endif  // DRACO_SCENE_AUDIO_H_
