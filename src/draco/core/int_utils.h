#ifndef DRACO_CORE_INT_UTILS_H_
#define DRACO_CORE_INT_UTILS_H_

#include <type_traits>

namespace draco {

// Performs the addition in unsigned type to avoid signed integer overflow. Note
// that the result will be the same (for non-overflowing values).
template <
    typename DataTypeT,
    typename std::enable_if<std::is_integral<DataTypeT>::value &&
                            std::is_signed<DataTypeT>::value>::type* = nullptr>
inline DataTypeT AddAsUnsigned(DataTypeT a, DataTypeT b) {
  typedef typename std::make_unsigned<DataTypeT>::type DataTypeUT;
  return static_cast<DataTypeT>(static_cast<DataTypeUT>(a) +
                                static_cast<DataTypeUT>(b));
}

// Fallback for types that are not signed integers (unsigned ints, floats, etc).
template <
    typename DataTypeT,
    typename std::enable_if<!std::is_integral<DataTypeT>::value ||
                            !std::is_signed<DataTypeT>::value>::type* = nullptr>
inline DataTypeT AddAsUnsigned(DataTypeT a, DataTypeT b) {
  return a + b;
}

// Performs the subtraction in unsigned type to avoid signed integer overflow.
// The result will be the same for non-overflowing values. Note that this cannot
// be replaced by AddAsUnsigned(a, -b) for b = *_MIN due to unsymmetric ranges
// of signed integers like [-128, 127] where -(-128) is not representable.
template <
    typename DataTypeT,
    typename std::enable_if<std::is_integral<DataTypeT>::value &&
                            std::is_signed<DataTypeT>::value>::type* = nullptr>
inline DataTypeT SubtractAsUnsigned(DataTypeT a, DataTypeT b) {
  typedef typename std::make_unsigned<DataTypeT>::type DataTypeUT;
  return static_cast<DataTypeT>(static_cast<DataTypeUT>(a) -
                                static_cast<DataTypeUT>(b));
}

// Fallback for types that are not signed integers (unsigned ints, floats, etc).
template <
    typename DataTypeT,
    typename std::enable_if<!std::is_integral<DataTypeT>::value ||
                            !std::is_signed<DataTypeT>::value>::type* = nullptr>
inline DataTypeT SubtractAsUnsigned(DataTypeT a, DataTypeT b) {
  return a - b;
}

// Performs the multiplication in unsigned type to avoid signed integer
// overflow. Note that the result will be the same (for non-overflowing values).
template <
    typename DataTypeT,
    typename std::enable_if<std::is_integral<DataTypeT>::value &&
                            std::is_signed<DataTypeT>::value>::type* = nullptr>
inline DataTypeT MultiplyAsUnsigned(DataTypeT a, DataTypeT b) {
  typedef typename std::make_unsigned<DataTypeT>::type DataTypeUT;
  return static_cast<DataTypeT>(static_cast<DataTypeUT>(a) *
                                static_cast<DataTypeUT>(b));
}

// Fallback for types that are not signed integers (unsigned ints, floats, etc).
template <
    typename DataTypeT,
    typename std::enable_if<!std::is_integral<DataTypeT>::value ||
                            !std::is_signed<DataTypeT>::value>::type* = nullptr>
inline DataTypeT MultiplyAsUnsigned(DataTypeT a, DataTypeT b) {
  return a * b;
}

}  // namespace draco

#endif  // DRACO_CORE_INT_UTILS_H_
