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
#include "compression/attributes/prediction_schemes/prediction_scheme_encoder_factory.h"

namespace draco {

// Returns the preferred prediction scheme based on the encoder options.
PredictionSchemeMethod GetPredictionMethodFromOptions(
    int att_id, const EncoderOptions &options) {
  const int pred_type =
      options.GetAttributeInt(att_id, "prediction_scheme", -1);
  if (pred_type == -1)
    return PREDICTION_UNDEFINED;
  if (pred_type < 0 || pred_type >= NUM_PREDICTION_SCHEMES)
    return PREDICTION_NONE;
  return static_cast<PredictionSchemeMethod>(pred_type);
}

}  // namespace draco
