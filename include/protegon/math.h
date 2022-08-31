#pragma once

#include <algorithm> // std::max

#include "type_traits.h"

namespace ptgn {

// Signum function.
// Returns  1  if value is positive.
// Returns  0  if value is zero.
// Returns -1  if value is negative.
// No NaN/inf checking.
template <typename T>
T Sign(T value) {
    return static_cast<T>((0 < value) - (value < 0));
}

// Source: https://stackoverflow.com/a/30308919.
// No NaN/inf checking.
template <typename T>
T FastFloor(T value) {
    if constexpr (std::is_floating_point_v<T>)
        return static_cast<T>((std::int64_t)value - (value < (std::int64_t)value));
    return value;
}

// No NaN/inf checking.
template <typename T>
T FastCeil(T value) {
    if constexpr (std::is_floating_point_v<T>)
        return static_cast<T>((std::int64_t)value + (value > (std::int64_t)value));
    return value;
}

// No NaN/inf checking.
template <typename T>
T FastAbs(T value) {
    return value >= 0 ? value : -value;
}

// Source: https://stackoverflow.com/a/65015333
// Compare two floating point numbers using relative tolerance and absolute tolerances.
// The absolute tolerance test fails when x and y become large.
// The relative tolerance test fails when x and y become small.
template <typename T>
bool NearlyEqual(T a, T b,
                 T rel_tol = 0.95f,
                 T abs_tol = 0.005f) {
    if constexpr (std::is_floating_point_v<T>)
        return a == b ||
        FastAbs(a - b) <= std::max(abs_tol, rel_tol * std::max(FastAbs(a), FastAbs(b)));
    else
        return a == b;
}

} // namespace ptgn