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
// Utilities for converting unit vectors to octaherdal coordinates and back.
// For more details about octahedral coordinates, see for example Cigolle
// et al.'14 “A Survey of Efficient Representations for Independent Unit
// Vectors”.

#ifndef DRACO_COMPRESSION_ATTRIBUTES_NORMAL_COMPRESSION_UTILS_H_
#define DRACO_COMPRESSION_ATTRIBUTES_NORMAL_COMPRESSION_UTILS_H_

#include <inttypes.h>

#include <algorithm>

namespace draco {

// Converts a unit vector into octahedral coordinates (0-1 range).
template <typename T>
void UnitVectorToOctahedralCoords(const T *vector, T *out_s, T *out_t) {
  const T abs_sum = fabs(vector[0]) + fabs(vector[1]) + fabs(vector[2]);
  T scaled_vec[3];
  if (abs_sum > 1e-6) {
    // Scale needed to project the vector to the surface of an octahedron.
    const T scale = 1.0 / abs_sum;
    scaled_vec[0] = vector[0] * scale;
    scaled_vec[1] = vector[1] * scale;
    scaled_vec[2] = vector[2] * scale;
  } else {
    scaled_vec[0] = 1;
    scaled_vec[1] = 0;
    scaled_vec[2] = 0;
  }

  if (scaled_vec[0] >= 0.0) {
    // Right hemisphere.
    *out_s = (scaled_vec[1] + 1.0) / 2.0;
    *out_t = (scaled_vec[2] + 1.0) / 2.0;
  } else {
    // Left hemisphere.
    if (scaled_vec[1] < 0.0) {
      *out_s = 0.5 * fabs(scaled_vec[2]);
    } else {
      *out_s = 0.5 * (2.0 - fabs(scaled_vec[2]));
    }
    if (scaled_vec[2] < 0.0) {
      *out_t = 0.5 * fabs(scaled_vec[1]);
    } else {
      *out_t = 0.5 * (2.0 - fabs(scaled_vec[1]));
    }
  }
}

template <typename T>
void UnitVectorToQuantizedOctahedralCoords(const T *vector,
                                           T max_quantized_value,
                                           int32_t *out_s, int32_t *out_t) {
  // In order to be able to represent the center normal we reduce the range
  // by one.
  const T max_value = max_quantized_value - 1;
  T ss, tt;
  UnitVectorToOctahedralCoords(vector, &ss, &tt);
  int32_t s = static_cast<int32_t>(floor(ss * max_value + 0.5));
  int32_t t = static_cast<int32_t>(floor(tt * max_value + 0.5));

  const int32_t center_value = static_cast<int32_t>(max_value / 2);

  // Convert all edge points in the top left and bottom right quadrants to
  // their corresponding position in the bottom left and top right quadrants.
  // Convert all corner edge points to the top right corner. This is necessary
  // for the inversion to occur correctly.
  if ((s == 0 && t == 0) || (s == 0 && t == max_value) ||
      (s == max_value && t == 0)) {
    s = static_cast<int32_t>(max_value);
    t = static_cast<int32_t>(max_value);
  } else if (s == 0 && t > center_value) {
    t = center_value - (t - center_value);
  } else if (s == max_value && t < center_value) {
    t = center_value + (center_value - t);
  } else if (t == max_value && s < center_value) {
    s = center_value + (center_value - s);
  } else if (t == 0 && s > center_value) {
    s = center_value - (s - center_value);
  }

  *out_s = s;
  *out_t = t;
}

template <typename T>
void OctaherdalCoordsToUnitVector(T in_s, T in_t, T *out_vector) {
  T s = in_s;
  T t = in_t;
  T spt = s + t;
  T smt = s - t;
  T x_sign = 1.0;
  if (spt >= 0.5 && spt <= 1.5 && smt >= -0.5 && smt <= 0.5) {
    // Right hemisphere. Don't do anything.
  } else {
    // Left hemisphere.
    x_sign = -1.0;
    if (spt <= 0.5) {
      s = 0.5 - in_t;
      t = 0.5 - in_s;
    } else if (spt >= 1.5) {
      s = 1.5 - in_t;
      t = 1.5 - in_s;
    } else if (smt <= -0.5) {
      s = in_t - 0.5;
      t = in_s + 0.5;
    } else {
      s = in_t + 0.5;
      t = in_s - 0.5;
    }
    spt = s + t;
    smt = s - t;
  }
  const T y = 2.0 * s - 1.0;
  const T z = 2.0 * t - 1.0;
  const T x = std::min(std::min(2.0 * spt - 1.0, 3.0 - 2.0 * spt),
                       std::min(2.0 * smt + 1.0, 1.0 - 2.0 * smt)) *
              x_sign;
  // Normalize the computed vector.
  const T normSquared = x * x + y * y + z * z;
  if (normSquared < 1e-6) {
    out_vector[0] = 0;
    out_vector[1] = 0;
    out_vector[2] = 0;
  } else {
    const T d = 1.0 / sqrt(normSquared);
    out_vector[0] = x * d;
    out_vector[1] = y * d;
    out_vector[2] = z * d;
  }
}

template <typename T>
void QuantizedOctaherdalCoordsToUnitVector(int32_t in_s, int32_t in_t,
                                           T max_quantized_value,
                                           T *out_vector) {
  // In order to be able to represent the center normal we reduce the range
  // by one. Also note that we can not simply identify the lower left and the
  // upper right edge of the tile, which forces us to use one value less.
  max_quantized_value -= 1;
  OctaherdalCoordsToUnitVector(in_s / max_quantized_value,
                               in_t / max_quantized_value, out_vector);
}

template <typename T>
bool IsInDiamond(const T &max_value_, const T &s, const T &t) {
  return std::abs(static_cast<double>(s)) + std::abs(static_cast<double>(t)) <=
         static_cast<double>(max_value_);
}

template <typename T>
void InvertRepresentation(const T &max_value_, T *s, T *t) {
  T sign_s = 0;
  T sign_t = 0;
  if (*s >= 0 && *t >= 0) {
    sign_s = 1;
    sign_t = 1;
  } else if (*s <= 0 && *t <= 0) {
    sign_s = -1;
    sign_t = -1;
  } else {
    sign_s = (*s > 0) ? 1 : -1;
    sign_t = (*t > 0) ? 1 : -1;
  }

  const T corner_point_s = sign_s * max_value_;
  const T corner_point_t = sign_t * max_value_;
  *s = 2 * *s - corner_point_s;
  *t = 2 * *t - corner_point_t;
  if (sign_s * sign_t >= 0) {
    T temp = *s;
    *s = -*t;
    *t = -temp;
  } else {
    std::swap(*s, *t);
  }
  *s = (*s + corner_point_s) / 2;
  *t = (*t + corner_point_t) / 2;
}

}  // namespace draco

#endif  // DRACO_COMPRESSION_ATTRIBUTES_NORMAL_COMPRESSION_UTILS_H_
