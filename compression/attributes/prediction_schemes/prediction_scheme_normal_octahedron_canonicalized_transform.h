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
#ifndef DRACO_COMPRESSION_ATTRIBUTES_PREDICTION_SCHEMES_PREDICTION_SCHEME_NORMAL_OCTAHEDRON_CANONICALIZED_TRANSFORM_H_
#define DRACO_COMPRESSION_ATTRIBUTES_PREDICTION_SCHEMES_PREDICTION_SCHEME_NORMAL_OCTAHEDRON_CANONICALIZED_TRANSFORM_H_

#include <cmath>

#include "compression/attributes/normal_compression_utils.h"
#include "compression/attributes/prediction_schemes/prediction_scheme.h"
#include "core/macros.h"
#include "core/vector_d.h"

namespace draco {

// The transform works on octahedral coordinates for normals. The square is
// subdivided into four inner triangles (diamond) and four outer triangles. The
// inner trianlges are associated with the upper part of the octahedron and the
// outer triangles are associated with the lower part.
// Given a preditiction value P and the actual value Q that should be encoded,
// this transform first checks if P is outside the diamond. If so, the outer
// triangles are flipped towards the inside and vice versa. Then it checks if p
// is in the bottom left quadrant. If it is not, it rotates p and q accordingly.
// The actual correction value is then based on the mapped and rotated P and Q
// values. The inversion tends to result in shorter correction vectors and the
// rotation makes it so that all long correction values are positive, reducing
// the possible value range of the correction values and increasing the
// occurence of positive large correction values, which helps the entropy
// encoder. This is possible since P is also known by the decoder, see also
// ComputeCorrection and ComputeOriginalValue functions.
// Note that the tile is not periodic, which implies that the outer edges can
// not be identified, which requires us to use an odd number of values on each
// axis.
// DataTypeT is expected to be some integral type.
//
template <typename DataTypeT>
class PredictionSchemeNormalOctahedronCanonicalizedTransform
    : public PredictionSchemeTransform<DataTypeT, DataTypeT> {
 public:
  typedef VectorD<DataTypeT, 2> Point2;
  typedef DataTypeT CorrType;
  typedef DataTypeT DataType;

  PredictionSchemeNormalOctahedronCanonicalizedTransform()
      : mod_value_(0), max_value_(0) {}
  // We expect the mod value to be of the form 2^b-1.
  PredictionSchemeNormalOctahedronCanonicalizedTransform(DataType mod_value)
      : mod_value_(mod_value), max_value_((mod_value - 1) / 2) {}

  PredictionSchemeTransformType GetType() const {
    return PREDICTION_TRANSFORM_NORMAL_OCTAHEDRON_CANONICALIZED;
  }

  // We can return true as we keep correction values positive.
  bool AreCorrectionsPositive() const { return true; }

  bool EncodeTransformData(EncoderBuffer *buffer) {
    buffer->Encode(mod_value_);
    buffer->Encode(max_value_);
    return true;
  }

  bool DecodeTransformData(DecoderBuffer *buffer) {
    if (!buffer->Decode(&mod_value_))
      return false;
    if (!buffer->Decode(&max_value_))
      return false;
    return true;
  }

  inline void ComputeCorrection(const DataType *orig_vals,
                                const DataType *pred_vals,
                                CorrType *out_corr_vals, int val_id) const {
    DCHECK_LE(pred_vals[0], max_value_ * 2);
    DCHECK_LE(pred_vals[1], max_value_ * 2);
    DCHECK_LE(orig_vals[0], max_value_ * 2);
    DCHECK_LE(orig_vals[1], max_value_ * 2);
    DCHECK_LE(0, pred_vals[0]);
    DCHECK_LE(0, pred_vals[1]);
    DCHECK_LE(0, orig_vals[0]);
    DCHECK_LE(0, orig_vals[1]);

    const Point2 orig = Point2(orig_vals[0], orig_vals[1]);
    const Point2 pred = Point2(pred_vals[0], pred_vals[1]);
    const Point2 corr = ComputeCorrection(orig, pred);
    DCHECK_EQ(true, Verify(orig, pred, corr));

    out_corr_vals[val_id] = corr[0];
    out_corr_vals[val_id + 1] = corr[1];
  }

  inline void ComputeOriginalValue(const DataType *pred_vals,
                                   const CorrType *corr_vals,
                                   DataType *out_orig_vals, int val_id) const {
    DCHECK_LE(pred_vals[0], 2 * max_value_);
    DCHECK_LE(pred_vals[1], 2 * max_value_);
    DCHECK_LE(corr_vals[val_id], 2 * max_value_);
    DCHECK_LE(corr_vals[val_id + 1], 2 * max_value_);

    DCHECK_LE(0, pred_vals[0]);
    DCHECK_LE(0, pred_vals[1]);
    DCHECK_LE(0, corr_vals[val_id]);
    DCHECK_LE(0, corr_vals[val_id + 1]);

    const Point2 pred = Point2(pred_vals[0], pred_vals[1]);
    const Point2 corr = Point2(corr_vals[val_id], corr_vals[val_id + 1]);
    const Point2 orig = ComputeOriginalValue(pred, corr);

    out_orig_vals[0] = orig[0];
    out_orig_vals[1] = orig[1];
  }

  int32_t GetRotationCount(Point2 pred) const {
    const DataType sign_x = pred[0];
    const DataType sign_y = pred[1];

    int32_t rotation_count = 0;
    if (sign_x == 0) {
      if (sign_y == 0) {
        rotation_count = 0;
      } else if (sign_y > 0) {
        rotation_count = 3;
      } else {
        rotation_count = 1;
      }
    } else if (sign_x > 0) {
      if (sign_y >= 0) {
        rotation_count = 2;
      } else {
        rotation_count = 1;
      }
    } else {
      if (sign_y <= 0) {
        rotation_count = 0;
      } else {
        rotation_count = 3;
      }
    }
    return rotation_count;
  }

  Point2 RotatePoint(Point2 p, int32_t rotation_count) const {
    switch (rotation_count) {
      case 1:
        return Point2(p[1], -p[0]);
      case 2:
        return Point2(-p[0], -p[1]);
      case 3:
        return Point2(-p[1], p[0]);
      default:
        return p;
    }
  }

  bool IsInBottomLeft(const Point2 &p) const {
    if (p[0] == 0 && p[1] == 0)
      return true;
    return (p[0] < 0 && p[1] <= 0);
  }

 private:
  Point2 ComputeCorrection(Point2 orig, Point2 pred) const {
    const Point2 t(max_value_, max_value_);
    orig = orig - t;
    pred = pred - t;
    if (!IsInDiamond(max_value_, pred[0], pred[1])) {
      InvertRepresentation(max_value_, &orig[0], &orig[1]);
      InvertRepresentation(max_value_, &pred[0], &pred[1]);
    }
    if (!IsInBottomLeft(pred)) {
      int32_t rotation_count = GetRotationCount(pred);
      orig = RotatePoint(orig, rotation_count);
      pred = RotatePoint(pred, rotation_count);
    }
    Point2 corr = orig - pred;
    corr[0] = MakePositive(corr[0]);
    corr[1] = MakePositive(corr[1]);
    return corr;
  }

  Point2 ComputeOriginalValue(Point2 pred, Point2 corr) const {
    const Point2 t(max_value_, max_value_);
    pred = pred - t;
    const bool pred_is_in_diamond = IsInDiamond(max_value_, pred[0], pred[1]);
    if (!pred_is_in_diamond) {
      InvertRepresentation(max_value_, &pred[0], &pred[1]);
    }
    const bool pred_is_in_bottom_left = IsInBottomLeft(pred);
    const int32_t rotation_count = GetRotationCount(pred);
    if (!pred_is_in_bottom_left) {
      pred = RotatePoint(pred, rotation_count);
    }
    Point2 orig = pred + corr;
    orig[0] = ModMax(orig[0]);
    orig[1] = ModMax(orig[1]);
    if (!pred_is_in_bottom_left) {
      const int32_t reverse_rotation_count = (4 - rotation_count) % 4;
      orig = RotatePoint(orig, reverse_rotation_count);
    }
    if (!pred_is_in_diamond) {
      InvertRepresentation(max_value_, &orig[0], &orig[1]);
    }
    orig = orig + t;
    return orig;
  }

  // For correction values.
  DataType MakePositive(DataType x) const {
    DCHECK_LE(x, max_value_ * 2);
    if (x < 0)
      return x + mod_value_;
    return x;
  }

  DataType ModMax(DataType x) const {
    if (x > max_value_)
      return x - mod_value_;
    if (x < -max_value_)
      return x + mod_value_;
    return x;
  }

  // Only called in debug mode.
  bool Verify(const Point2 &orig, const Point2 pred, const Point2 corr) const {
    const Point2 veri = ComputeOriginalValue(pred, corr);
    return AreEquivalent(orig, veri);
  }

  // Only called in debug mode
  bool AreEquivalent(Point2 p, Point2 q) const {
    const Point2 t(max_value_, max_value_);
    p = p - t;
    q = q - t;
    if (std::abs(p[0]) == max_value_ && p[1] < 0)
      p[1] = -p[1];
    if (std::abs(p[1]) == max_value_ && p[0] < 0)
      p[0] = -p[0];
    if (std::abs(q[0]) == max_value_ && q[1] < 0)
      q[1] = -q[1];
    if (std::abs(q[1]) == max_value_ && q[0] < 0)
      q[0] = -q[0];
    return (p[0] == q[0] && p[1] == q[1]);
  }

  DataType mod_value_;
  DataType max_value_;
};

}  // namespace draco

#endif  // DRACO_COMPRESSION_ATTRIBUTES_PREDICTION_SCHEMES_PREDICTION_SCHEME_NORMAL_OCTAHEDRON_CANONICALIZED_TRANSFORM_H_
