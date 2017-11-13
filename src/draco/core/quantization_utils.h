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
// A set of classes for quantizing and dequantizing of floating point values
// into integers.
// The quantization works on all floating point numbers within (-range, +range)
// interval producing integers in range
// (-max_quantized_value, +max_quantized_value).

#ifndef DRACO_CORE_QUANTIZATION_UTILS_H_
#define DRACO_CORE_QUANTIZATION_UTILS_H_

#include <stdint.h>
#include <cmath>

#include "draco/core/macros.h"

namespace draco {

// Class for quantizing single precision floating point values. The values must
// be centered around zero and be within interval (-range, +range), where the
// range is specified in the Init() method.
class Quantizer {
 public:
  Quantizer();
  void Init(float range, int32_t max_quantized_value);
  inline int32_t QuantizeFloat(float val) const {
    const bool neg = (val < 0.f);
    if (neg) {
      val = -val;
    }
    val /= range_;
    const int32_t ret = static_cast<int32_t>(
        floor(val * static_cast<float>(max_quantized_value_) + 0.5f));
    if (neg)
      return -ret;
    return ret;
  }
  inline int32_t operator()(float val) const { return QuantizeFloat(val); }

 private:
  float range_;
  int32_t max_quantized_value_;
};

// Class for dequantizing values that were previously quantized using the
// Quantizer class.
class Dequantizer {
 public:
  Dequantizer();

  // Initializes the dequantizer. Both parameters must correspond to the values
  // provided to the initializer of the Quantizer class.
  // Returns false when the initialization fails.
  bool Init(float range, int32_t max_quantized_value);
  inline float DequantizeFloat(int32_t val) const {
    const bool neg = (val < 0);
    if (neg) {
      val = -val;
    }
    float norm_value = static_cast<float>(val) * max_quantized_value_factor_;
    if (neg)
      norm_value = -norm_value;
    return norm_value * range_;
  }
  inline float operator()(int32_t val) const { return DequantizeFloat(val); }

 private:
  float range_;
  // Distance between two normalized dequantized values.
  float max_quantized_value_factor_;
};

}  // namespace draco

#endif  // DRACO_CORE_QUANTIZATION_UTILS_H_
