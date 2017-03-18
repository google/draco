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
#ifndef DRACO_CORE_VECTOR_D_H_
#define DRACO_CORE_VECTOR_D_H_

#include <inttypes.h>
#include <algorithm>
#include <array>
#include <cmath>

#include "core/macros.h"

namespace draco {
// D-dimensional vector class with basic operations.
template <class CoeffT, int dimension_t>
class VectorD {
 public:
  typedef VectorD<CoeffT, dimension_t> Self;
  typedef CoeffT CoefficientType;
  static constexpr int dimension = dimension_t;

  VectorD() {
    for (int i = 0; i < dimension_t; ++i)
      (*this)[i] = CoeffT(0);
  }

  // The following constructor does not compile in opt mode, which for now led
  // to the constructors further down, which is not ideal.
  // TODO(hemmer): fix constructor below and remove others.
  // template <typename... Args>
  // explicit VectorD(Args... args) : v_({args...}) {}

  VectorD(const CoeffT &c0, const CoeffT &c1) : v_({{c0, c1}}) {
    CHECK_EQ(dimension_t, 2);
    v_[0] = c0;
    v_[1] = c1;
  }

  VectorD(const CoeffT &c0, const CoeffT &c1, const CoeffT &c2)
      : v_({{c0, c1, c2}}) {
    CHECK_EQ(dimension_t, 3);
  }

  VectorD(const CoeffT &c0, const CoeffT &c1, const CoeffT &c2,
          const CoeffT &c3)
      : v_({{c0, c1, c2, c3}}) {
    CHECK_EQ(dimension_t, 4);
  }

  VectorD(const CoeffT &c0, const CoeffT &c1, const CoeffT &c2,
          const CoeffT &c3, const CoeffT &c4)
      : v_({{c0, c1, c2, c3, c4}}) {
    CHECK_EQ(dimension_t, 5);
  }

  VectorD(const Self &o) {
    for (int i = 0; i < dimension_t; ++i)
      (*this)[i] = o[i];
  }

  CoeffT &operator[](int i) { return v_[i]; }
  const CoeffT &operator[](int i) const { return v_[i]; }
  // TODO(hemmer): remove.
  // Similar to interface of Eigen library.
  CoeffT &operator()(int i) { return v_[i]; }
  const CoeffT &operator()(int i) const { return v_[i]; }

  // Unary operators.
  Self operator-() const {
    Self ret;
    for (int i = 0; i < dimension_t; ++i) {
      ret[i] = -(*this)[i];
    }
    return ret;
  }

  // Binary operators.
  Self operator+(const Self &o) const {
    Self ret;
    for (int i = 0; i < dimension_t; ++i) {
      ret[i] = (*this)[i] + o[i];
    }
    return ret;
  }

  Self operator-(const Self &o) const {
    Self ret;
    for (int i = 0; i < dimension_t; ++i) {
      ret[i] = (*this)[i] - o[i];
    }
    return ret;
  }

  Self operator*(const CoeffT &o) const {
    Self ret;
    for (int i = 0; i < dimension_t; ++i) {
      ret[i] = (*this)[i] * o;
    }
    return ret;
  }

  Self operator/(const CoeffT &o) const {
    Self ret;
    for (int i = 0; i < dimension_t; ++i) {
      ret[i] = (*this)[i] / o;
    }
    return ret;
  }

  bool operator==(const Self &o) const {
    for (int i = 0; i < dimension_t; ++i) {
      if ((*this)[i] != o[i])
        return false;
    }
    return true;
  }

  bool operator!=(const Self &x) const { return !((*this) == x); }

  bool operator<(const Self &x) const {
    for (int i = 0; i < dimension_t - 1; ++i) {
      if (v_[i] < x.v_[i])
        return true;
      if (v_[i] > x.v_[i])
        return false;
    }
    // Only one check needed for the last dimension.
    if (v_[dimension_t - 1] < x.v_[dimension_t - 1])
      return true;
    return false;
  }

  // Functions.
  CoeffT SquaredNorm() const { return this->Dot(*this); }
  CoeffT Dot(const Self &o) const {
    CoeffT ret(0);
    for (int i = 0; i < dimension_t; ++i) {
      ret += (*this)[i] * o[i];
    }
    return ret;
  }
  void Normalize() {
    const CoeffT magnitude = sqrt(this->SquaredNorm());
    if (magnitude == 0) {
      return;
    }
    for (int i = 0; i < dimension_t; ++i) {
      (*this)[i] /= magnitude;
    }
  }
  CoeffT *data() { return &(v_[0]); }

 private:
  std::array<CoeffT, dimension_t> v_;
};

// Scalar multiplication from the other side too.
template <class CoeffT, int dimension_t>
VectorD<CoeffT, dimension_t> operator*(const CoeffT &o,
                                       const VectorD<CoeffT, dimension_t> &v) {
  return v * o;
}

// Calculates the squared distance between two points.
template <class CoeffT, int dimension_t>
CoeffT SquaredDistance(const VectorD<CoeffT, dimension_t> v1,
                       const VectorD<CoeffT, dimension_t> v2) {
  CoeffT difference;
  CoeffT squared_distance = 0;
  // Check each index seperately so difference is never negative and underflow
  // is avoided for unsigned types.
  for (int i = 0; i < dimension_t; ++i) {
    if (v1[i] >= v2[i]) {
      difference = v1[i] - v2[i];
    } else {
      difference = v2[i] - v1[i];
    }
    squared_distance += (difference * difference);
  }
  return squared_distance;
}

typedef VectorD<float, 2> Vector2f;
typedef VectorD<float, 3> Vector3f;
typedef VectorD<float, 4> Vector4f;
typedef VectorD<float, 5> Vector5f;

typedef VectorD<uint32_t, 2> Vector2ui;
typedef VectorD<uint32_t, 3> Vector3ui;
typedef VectorD<uint32_t, 4> Vector4ui;
typedef VectorD<uint32_t, 5> Vector5ui;

}  // namespace draco

template <typename Char, typename CharTraits, class CoeffT, int dimension_t>
inline ::std::basic_ostream<Char, CharTraits> &operator<<(
    ::std::basic_ostream<Char, CharTraits> &out,
    const draco::VectorD<CoeffT, dimension_t> &vec) {
  for (int i = 0; i < dimension_t - 1; ++i) {
    out << vec[i] << " ";
  }
  out << vec[dimension_t - 1];
  return out;
}

#endif  // DRACO_CORE_VECTOR_D_H_
