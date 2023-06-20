#pragma once

#include <algorithm> // std::max
#include <cmath>     // std::sqrtf, std::fmod
#include <tuple>     // std::tuple

#include "type_traits.h"

namespace ptgn {

namespace impl {

template <typename T, type_traits::floating_point<T> = true> class Pi {};
template <typename T, type_traits::floating_point<T> = true> class TwoPi {};
template <typename T, type_traits::floating_point<T> = true> class HalfPi {};
template<> class Pi<float>      { public: static constexpr float  value() { return 3.14159265; } };
template<> class Pi<double>     { public: static constexpr double value() { return 3.141592653589793; } };
template<> class TwoPi<float>   { public: static constexpr float  value() { return 6.2831853; } };
template<> class TwoPi<double>  { public: static constexpr double value() { return 6.283185307179586; } };
template<> class HalfPi<float>  { public: static constexpr float  value() { return 1.570796325; } };
template<> class HalfPi<double> { public: static constexpr double value() { return 1.5707963267948965; } };

} // namespace impl

template <typename T = float, type_traits::floating_point<T> = true>
inline constexpr T pi{ impl::Pi<T>::value() };
template <typename T = float, type_traits::floating_point<T> = true>
inline constexpr T two_pi{ impl::TwoPi<T>::value() };
template <typename T = float, type_traits::floating_point<T> = true>
inline constexpr T half_pi{ impl::HalfPi<T>::value() };
template <typename T = float>
inline constexpr T epsilon{ std::numeric_limits<T>::epsilon() };
template <typename T = float>
inline constexpr T epsilon2{ epsilon<T> * epsilon<T> };

// Convert degrees to radians.
template <typename T, type_traits::floating_point<T> = true>
T ToRad(T deg) {
    return deg * pi<T> / 180;
}

// Convert radians to degrees.
template <typename T, type_traits::floating_point<T> = true>
T ToDeg(T rad) {
    return rad / pi<T> * 180;
}

// Angle in degrees clamped from 0 to 360.
template <typename T, type_traits::arithmetic<T> = true>
T ClampAngle360(T deg) {
    if constexpr (std::is_floating_point_v<T>) {
        deg = std::fmod(deg, 360);
        while (deg < 0)
            deg += 360;
        deg = std::fmod(deg, 360);
    } else {
        deg %= 360;
        while (deg < 0)
            deg += 360;
        deg %= 360;
    }
    return deg;
}

// Angle in radians clamped from 0 to 2 pi.
template <typename T, type_traits::floating_point<T> = true>
T ClampAngle2Pi(T rad) {
    rad = std::fmod(rad, two_pi<T>);
    while (rad < 0)
        rad += two_pi<T>;
    rad = std::fmod(rad, two_pi<T>);
    return rad;
}

// Signum function.
// Returns  1  if value is positive.
// Returns  0  if value is zero.
// Returns -1  if value is negative.
// No NaN/inf checking.
template <typename T>
T Sign(T value) {
    return static_cast<T>((0 < value) - (value < 0));
}

// Returns a wrapped to mod n in positive and negative directions.
inline int ModFloor(int a, int n) {
    return ((a % n) + n) % n;
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
                 T rel_tol = 10.0f * epsilon<float>,
                 T abs_tol = 0.005f) {
    if constexpr (std::is_floating_point_v<T>)
        return a == b ||
        FastAbs(a - b) <= std::max(abs_tol, rel_tol * std::max(FastAbs(a), FastAbs(b)));
    else
        return a == b;
}

// Returns true if there is a real solution followed by both roots
// (equal if repeated), false and roots of 0 if imaginary.
template <typename T, type_traits::floating_point<T> = true>
std::tuple<bool, T, T> QuadraticFormula(T a, T b, T c) {
    const T disc{ b * b - 4.0f * a * c };
    if (disc < 0.0f) {
        // Imaginary roots.
        return { false, 0.0f, 0.0f };
    } else if (NearlyEqual(disc, static_cast<T>(0))) {
        // Repeated roots.
        const T root{ -0.5f * b / a };
        return { true, root, root };
    }
    // Real roots.
    const T q = (b > 0.0f) ?
        -0.5f * (b + std::sqrtf(disc)) :
        -0.5f * (b - std::sqrtf(disc));
    // This may look weird but the algebra checks out here (I checked).
    return { true, q / a, c / q };
}

template <typename T, typename U,
    type_traits::arithmetic<T> = true,
    type_traits::floating_point<U> = true>
T Lerp(T a, T b, U t) {
    return a + t * (b - a);
}

} // namespace ptgn