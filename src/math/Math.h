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

template <typename T,
    tt::arithmetic<T> = true>
class Epsilon {
public:
    static constexpr T value() { return 0; }
};

template<> class Epsilon<float> {
public:
    static constexpr float value() { return 1.0e-5f; }
};

template<> class Epsilon<double> {
public:
    static constexpr double value() { return 1.0e-10; }
};

template <typename T,
    tt::floating_point<T> = true>
class Pi {};

template<> class Pi<float> {
public:
    static constexpr float value() { return 3.14159265; }
};

template<> class Pi<double> {
public:
    static constexpr double value() { return 3.141592653589793; }
};

template <typename T,
    tt::floating_point<T> = true>
class TwoPi {};

template<> class TwoPi<float> {
public:
    static constexpr float value() { return 6.2831853; }
};

template<> class TwoPi<double> {
public:                                      
    static constexpr double value() { return 6.283185307179586; }
};

template <typename T,
    tt::floating_point<T> = true>
class HalfPi {};

template<> class HalfPi<float> {
public:
    static constexpr float value() { return 1.570796325; }
};

template<> class HalfPi<double> {
public:
    static constexpr double value() { return 1.5707963267948965; }
};

// Source: https://stackoverflow.com/a/65015333
template <typename T, typename U,
    tt::arithmetic<T> = true,
    tt::arithmetic<U> = true,
    typename S = typename std::common_type_t<T, U>>
static constexpr S scale_epsilon(T a, U b, S epsilon = epsilon<S>) noexcept {
    S scaling_factor{};
    // Special case for when a or b is infinity
    if (std::isinf(static_cast<S>(a)) || std::isinf(static_cast<S>(b))) {
        scaling_factor = 0;
    } else {
        scaling_factor = std::max({ S{ 1 },
                                  static_cast<S>(std::abs(a)),
                                  static_cast<S>(std::abs(b)) });
    }
    S epsilon_scaled{ scaling_factor * std::abs(epsilon) };
    return epsilon_scaled;
}

} // namespace impl

template <typename T = float,
    tt::floating_point<T> = true>
inline constexpr T pi{ impl::Pi<T>::value() };
template <typename T = float,
    tt::floating_point<T> = true>
inline constexpr T two_pi{ impl::TwoPi<T>::value() };
template <typename T = float,
    tt::floating_point<T> = true>
inline constexpr T half_pi{ impl::HalfPi<T>::value() };
// TODO: CONSIDER: Is changing value to std::numeric_limits<T>::epsilon() better?
// Some collision require it to be higher to work correctly.
template <typename T = float,
    tt::arithmetic<T> = true>
inline constexpr T epsilon{ impl::Epsilon<T>::value() };

// Convert degrees to radians.
template <typename T, 
    tt::floating_point<T> = true>
inline T ToRad(T deg) {
	return deg * pi<T> / 180;
}

// Convert radians to degrees.
template <typename T,
    tt::floating_point<T> = true>
inline T ToDeg(T rad) {
    return rad / pi<T> * 180;
}

// Angle in degrees clamped from 0 to 360.
template <typename T,
    tt::arithmetic<T> = true>
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
template <typename T,
    tt::floating_point<T> = true>
T ClampAngle2Pi(T rad) {
    rad = std::fmod(rad, two_pi<T>);
    while (rad < 0) rad += two_pi<T>;
    rad = std::fmod(rad, two_pi<T>);
    return rad;
}

// Signum function.
// Returns  1  if value is positive.
// Returns  0  if value is zero.
// Returns -1  if value is negative.
template <typename T, 
    tt::arithmetic<T> = true>
inline T Sign(T value) {
    return static_cast<T>((0 < value) - (value < 0));
}

// Source: https://stackoverflow.com/a/30308919.
// No range-checking.
template <typename T,
    tt::arithmetic<T> = true>
inline T FastFloor(T value) {
    if constexpr (std::is_floating_point_v<T>)
        return static_cast<T>((std::int64_t)value - (value < (std::int64_t)value));
    return value;
}

// No range-checking.
template <typename T,
    tt::arithmetic<T> = true>
inline T FastCeil(T value) {
    if constexpr (std::is_floating_point_v<T>)
        return static_cast<T>((std::int64_t)value + (value > (std::int64_t)value));
    return value;
}

// No range-checking.
template <typename T, 
    tt::arithmetic<T> = true>
inline T FastAbs(T value) {
    return (value < 0) ? -value : value;
}

// TODO: Check that this works without casts.
template <typename T, 
    tt::arithmetic<T> = true>
inline T SmoothStep(T value) {
    return value * value * (3 - 2 * value);
}

// Source: https://stackoverflow.com/a/65015333
// Compare two floating point numbers using relative tolerance and absolute tolerances.
// The absolute tolerance test fails when x and y become large.
// The relative tolerance test fails when x and y become small.
template <typename T, typename U,
    tt::arithmetic<T> = true,
    tt::arithmetic<U> = true,
    typename S = typename std::common_type_t<T, U>>
inline constexpr bool Compare(T a, U b, S epsilon = math::epsilon<S>) noexcept {
    if constexpr (std::is_floating_point_v<S>)
        return a == b || std::abs(a - b) <= impl::scale_epsilon(a, b, epsilon);
    return a == b;
}

// Returns true if there is a real solution followed by both roots
// (equal if repeated), false and roots of 0 if imaginary.
template <typename T,
    tt::floating_point<T> = true>
static std::tuple<bool, T, T> QuadraticFormula(T a, T b, T c) {
    const T discr{ b * b - 4.0f * a * c };
    if (discr < 0) {
        // Imaginary roots.
        return { false, 0, 0 };
    } else if (math::Compare(discr, 0)) {
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

} // namespace ptgn