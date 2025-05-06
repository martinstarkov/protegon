#pragma once

#include <cstdint>
#include <limits>
#include <type_traits>
#include <utility>

#include "common/type_traits.h"

namespace ptgn {

namespace impl {

template <typename T, tt::floating_point<T> = true>
class Pi {};

template <typename T, tt::floating_point<T> = true>
class TwoPi {};

template <typename T, tt::floating_point<T> = true>
class HalfPi {};

template <typename T, tt::floating_point<T> = true>
class SqrtTwo {};

template <>
class Pi<float> {
public:
	static constexpr float value() {
		return 3.14159265f;
	}
};

template <>
class Pi<double> {
public:
	static constexpr double value() {
		return 3.141592653589793;
	}
};

template <>
class TwoPi<float> {
public:
	static constexpr float value() {
		return 6.2831853f;
	}
};

template <>
class TwoPi<double> {
public:
	static constexpr double value() {
		return 6.283185307179586;
	}
};

template <>
class HalfPi<float> {
public:
	static constexpr float value() {
		return 1.570796325f;
	}
};

template <>
class HalfPi<double> {
public:
	static constexpr double value() {
		return 1.5707963267948965;
	}
};

template <>
class SqrtTwo<float> {
public:
	static constexpr float value() {
		return 1.414213563f;
	}
};

template <>
class SqrtTwo<double> {
public:
	static constexpr double value() {
		return 1.4142135623730951;
	}
};

} // namespace impl

template <typename T = float, tt::floating_point<T> = true>
inline constexpr T pi{ impl::Pi<T>::value() };
template <typename T = float, tt::floating_point<T> = true>
inline constexpr T two_pi{ impl::TwoPi<T>::value() };
template <typename T = float, tt::floating_point<T> = true>
inline constexpr T half_pi{ impl::HalfPi<T>::value() };
template <typename T = float, tt::floating_point<T> = true>
inline constexpr T sqrt_two{ impl::SqrtTwo<T>::value() };
template <typename T = float>
inline constexpr T epsilon{ std::numeric_limits<T>::epsilon() };
template <typename T = float>
inline constexpr T epsilon2{ epsilon<T> * epsilon<T> };

// Convert degrees to radians.
template <typename T, tt::floating_point<T> = true>
[[nodiscard]] constexpr T DegToRad(T angle_degrees) {
	return angle_degrees * pi<T> / T{ 180 };
}

// Convert radians to degrees.
template <typename T, tt::floating_point<T> = true>
[[nodiscard]] constexpr T RadToDeg(T angle_radians) {
	return angle_radians / pi<T> * T{ 180 };
}

// Modulo operator which supports wrapping negative numbers.
// e.g. Mod(-1, 2) returns 1.
template <typename T, tt::integral<T> = true>
[[nodiscard]] T Mod(T a, T b) {
	return (a % b + b) % b;
}

// Angle in degrees from [0, 360).
template <typename T, tt::arithmetic<T> = true>
[[nodiscard]] T ClampAngle360(T angle_degrees) {
	T clamped{ 0 };

	if constexpr (std::is_floating_point_v<T>) {
		clamped = std::fmod(angle_degrees, T{ 360 });
	} else {
		clamped = Mod(angle_degrees, T{ 360 });
	}

	if (clamped < 0) {
		clamped += T{ 360 };
	}

	return clamped;
}

// @return Angle in radians in range [0, 2 pi).
template <typename T, tt::floating_point<T> = true>
[[nodiscard]] T ClampAngle2Pi(T angle_radians) {
	T clamped{ std::fmod(angle_radians, two_pi<T>) };

	if (clamped < 0.0) {
		clamped += two_pi<T>;
	}

	return clamped;
}

// Signum function.
// Returns  1  if value is positive.
// Returns  0  if value is zero.
// Returns -1  if value is negative.
// No NaN/inf checking.
template <typename T>
[[nodiscard]] T Sign(T value) {
	return static_cast<T>((0 < value) - (value < 0));
}

// Returns a wrapped to mod n in positive and negative directions.
[[nodiscard]] inline int ModFloor(int a, int n) {
	return ((a % n) + n) % n;
}

// Source: https://stackoverflow.com/a/30308919.
// No NaN/inf checking.
template <typename T>
[[nodiscard]] T FastFloor(T value) {
	if constexpr (std::is_floating_point_v<T>) {
		return static_cast<T>(
			static_cast<std::int64_t>(value) - (value < static_cast<std::int64_t>(value))
		);
	} else {
		return value;
	}
}

// No NaN/inf checking.
template <typename T>
[[nodiscard]] T FastRound(T value) {
	if constexpr (std::is_floating_point_v<T>) {
		return FastFloor(value + 0.5f);
	} else {
		return value;
	}
}

// No NaN/inf checking.
template <typename T>
[[nodiscard]] T FastCeil(T value) {
	if constexpr (std::is_floating_point_v<T>) {
		return static_cast<T>(
			static_cast<std::int64_t>(value) + (value > static_cast<std::int64_t>(value))
		);
	} else {
		return value;
	}
}

// No NaN/inf checking.
template <typename T>
[[nodiscard]] T FastAbs(T value) noexcept {
	return value >= 0 ? value : -value;
}

template <typename T>
[[nodiscard]] T FastMin(T a, T b) {
	return a < b ? a : b;
}

template <typename T>
[[nodiscard]] T FastMax(T a, T b) {
	return a > b ? a : b;
}

// Source: https://stackoverflow.com/a/65015333
// Compare two floating point numbers using relative tolerance and absolute
// tolerances. The absolute tolerance test fails when x and y become large. The
// relative tolerance test fails when x and y become small.
template <typename T>
[[nodiscard]] bool
NearlyEqual(T a, T b, T abs_tol = T{ 10 } * epsilon<T>, T rel_tol = T{ 10 } * epsilon<T>) noexcept {
	if constexpr (std::is_floating_point_v<T>) {
		// TODO: Fix this.
		bool a_inf{ std::isinf(a) };
		bool b_inf{ std::isinf(b) };
		if (a_inf || b_inf) {
			if (a_inf && b_inf) {
				return true;
			}
			return false;
		}
		return a == b ||
			   FastAbs(a - b) <= std::max(abs_tol, rel_tol * std::max(FastAbs(a), FastAbs(b)));
	} else {
		return a == b;
	}
}

// Returns true if there is a real solution followed by both roots
// (equal if repeated), false and roots of 0 if imaginary.
template <typename T, tt::floating_point<T> = true>
[[nodiscard]] std::tuple<bool, T, T> QuadraticFormula(T a, T b, T c) {
	const T disc{ b * b - 4.0f * a * c };
	if (disc < 0.0f) {
		// Imaginary roots.
		return { false, 0.0f, 0.0f };
	} else if (NearlyEqual(disc, T{ 0 })) {
		// Repeated roots.
		const T root{ -0.5f * b / a };
		return { true, root, root };
	}
	// Real roots.
	const T q = (b > 0.0f) ? -0.5f * (b + std::sqrt(disc)) : -0.5f * (b - std::sqrt(disc));
	// This may look weird but the algebra checks out here (I checked).
	return { true, q / a, c / q };
}

template <typename T, typename U, tt::arithmetic<T> = true, tt::floating_point<U> = true>
[[nodiscard]] U Lerp(T a, T b, U t) {
	return a + t * (b - a);
}

template <typename T, typename U, tt::arithmetic<T> = true, tt::floating_point<U> = true>
[[nodiscard]] U CosineInterpolate(T a, T b, U t) {
	return Lerp(a, b, static_cast<U>(0.5) * (static_cast<U>(1) - std::cos(t * pi<U>)));
}

// From https://paulbourke.net/miscellaneous/interpolation/
template <typename T, typename U, tt::arithmetic<T> = true, tt::floating_point<U> = true>
[[nodiscard]] U CubicInterpolate(T y0, T y1, T y2, T y3, U t) {
	U mu2 = t * t;
	U a0  = y3 - y2 - y0 + y1;
	U a1  = y0 - y1 - a0;
	U a2  = y2 - y0;
	U a3  = y1;
	return (a0 * t * mu2 + a1 * mu2 + a2 * t + a3);
}

template <typename U, tt::floating_point<U> = true>
[[nodiscard]] U Quintic(U t) {
	return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

template <typename U, tt::floating_point<U> = true>
[[nodiscard]] U QuinticInterpolate(U a, U b, U t) {
	return Lerp(a, b, Quintic(t));
}

template <typename U, tt::floating_point<U> = true>
[[nodiscard]] U Smoothstep(U t) {
	return t * t * (3.0f - 2.0f * t);
}

// From: https://en.wikipedia.org/wiki/Smoothstep
template <typename U, tt::floating_point<U> = true>
[[nodiscard]] U SmoothstepInterpolate(U a, U b, U t) {
	return Lerp(a, b, Smoothstep(t));
}

} // namespace ptgn
