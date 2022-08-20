#pragma once

// This header filer contains standard math functions that are used frequently.
// In some cases with improved performance over standard library alternatives, e.g. ceil and floor.

#include <cstdlib>   // std::size_t
#include <cmath>     // std::sqrtf
#include <algorithm> // std::max
#include <tuple>     // std::tuple

#include "utility/TypeTraits.h"

namespace ptgn {

namespace math {

namespace impl {

template <typename T, tt::floating_point<T> = true> class Pi {};
template <typename T, tt::floating_point<T> = true> class TwoPi {};
template <typename T, tt::floating_point<T> = true> class HalfPi {};
template<> class Pi<float>      { public: static constexpr float  value() { return 3.14159265; } };
template<> class Pi<double>     { public: static constexpr double value() { return 3.141592653589793; } };
template<> class TwoPi<float>   { public: static constexpr float  value() { return 6.2831853; } };
template<> class TwoPi<double>  { public: static constexpr double value() { return 6.283185307179586; } };
template<> class HalfPi<float>  { public: static constexpr float  value() { return 1.570796325; } };
template<> class HalfPi<double> { public: static constexpr double value() { return 1.5707963267948965; } };

} // namespace impl

template <typename T = float, tt::floating_point<T> = true>
inline constexpr T pi{ impl::Pi<T>::value() };
template <typename T = float, tt::floating_point<T> = true>
inline constexpr T two_pi{ impl::TwoPi<T>::value() };
template <typename T = float, tt::floating_point<T> = true>
inline constexpr T half_pi{ impl::HalfPi<T>::value() };

// Convert degrees to radians.
template <typename T, tt::floating_point<T> = true>
T ToRad(T deg) {
	return deg * pi<T> / 180;
}

// Convert radians to degrees.
template <typename T, tt::floating_point<T> = true>
T ToDeg(T rad) {
    return rad / pi<T> * 180;
}

// Angle in degrees clamped from 0 to 360.
template <typename T, tt::arithmetic<T> = true>
T ClampAngle360(T deg) {
    if constexpr (std::is_floating_point_v<T>) {
        deg = std::fmod(deg, 360);
        while (deg < 0) deg += 360;
        deg = std::fmod(deg, 360);
    } else {
        deg %= 360;
        while (deg < 0) deg += 360;
        deg %= 360;
    }
    return deg;
}

// Angle in radians clamped from 0 to 2 pi.
template <typename T, tt::floating_point<T> = true>
T ClampAngle2Pi(T rad) {
    rad = std::fmod(rad, two_pi<T>);
    while (rad < 0) rad += two_pi<T>;
    rad = std::fmod(rad, two_pi<T>);
    return rad;
}

// Returns true if there is a real solution followed by both roots
// (equal if repeated), false and roots of 0 if imaginary.
template <typename T, tt::floating_point<T> = true>
std::tuple<bool, T, T> QuadraticFormula(T a, T b, T c) {
    const T discr{ b * b - 4.0f * a * c };
    if (discr < 0) {
        // Imaginary roots.
        return { false, 0, 0 };
    } else if (Compare(discr, 0)) {
        // Repeated roots.
        T root{ -0.5f * b / a };
        return { true, root, root };
    }
    // Real roots.
    const T q = (b > 0) ?
        -0.5f * (b + std::sqrtf(discr)) :
        -0.5f * (b - std::sqrtf(discr));
    return { true, q / a, c / q };
}

} // namespace math

template <typename T = float>
inline constexpr T epsilon{ std::numeric_limits<T>::epsilon() };

template <typename T = float>
inline constexpr T epsilon2{ epsilon<T> * epsilon<T> };

// Signum function.
// Returns  1  if value is positive.
// Returns  0  if value is zero.
// Returns -1  if value is negative.
template <typename T, tt::arithmetic<T> = true>
T Sign(T value) {
    return static_cast<T>((0 < value) - (value < 0));
}

// Source: https://stackoverflow.com/a/30308919.
// No range-checking.
template <typename T, tt::arithmetic<T> = true>
T FastFloor(T value) {
    if constexpr (std::is_floating_point_v<T>)
        return static_cast<T>((std::int64_t)value - (value < (std::int64_t)value));
    return value;
}

// No range-checking.
template <typename T, tt::arithmetic<T> = true>
T FastCeil(T value) {
    if constexpr (std::is_floating_point_v<T>)
        return static_cast<T>((std::int64_t)value + (value > (std::int64_t)value));
    return value;
}

// No range-checking.
template <typename T, tt::arithmetic<T> = true>
T FastAbs(T value) {
    return value >= 0 ? value : -value;
}

// TODO: Check that this works without casts.
template <typename T, tt::arithmetic<T> = true>
T SmoothStep(T value) {
    return value * value * (3 - 2 * value);
}

// Source: https://stackoverflow.com/a/65015333
// Compare two floating point numbers using relative tolerance and absolute tolerances.
// The absolute tolerance test fails when x and y become large.
// The relative tolerance test fails when x and y become small.
template <typename T,
    tt::floating_point<T> = true>
bool NearlyEqual(T a, T b,
                 T rel_tol = 0.95f,
                 T abs_tol = 0.01f) {
    return a == b || FastAbs(a - b) <= std::max(abs_tol, rel_tol * std::max(FastAbs(a), FastAbs(b)));
}


} // namespace ptgn