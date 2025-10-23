#pragma once

#include <concepts>
#include <cstdint>
#include <tuple>
#include <type_traits>

#include "core/utils/concepts.h"
#include "debug/runtime/assert.h"
#include "math/tolerance.h"

namespace ptgn {

namespace impl {

template <std::floating_point T>
class Pi {};

template <std::floating_point T>
class TwoPi {};

template <std::floating_point T>
class HalfPi {};

template <std::floating_point T>
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

template <std::floating_point T = float>
inline constexpr T pi{ impl::Pi<T>::value() };

template <std::floating_point T = float>
inline constexpr T two_pi{ impl::TwoPi<T>::value() };

template <std::floating_point T = float>
inline constexpr T half_pi{ impl::HalfPi<T>::value() };

template <std::floating_point T = float>
inline constexpr T sqrt_two{ impl::SqrtTwo<T>::value() };

// Convert degrees to radians.
template <std::floating_point T>
[[nodiscard]] constexpr T DegToRad(T angle_degrees) {
	return angle_degrees * pi<T> / T{ 180 };
}

// Convert radians to degrees.
template <std::floating_point T>
[[nodiscard]] constexpr T RadToDeg(T angle_radians) {
	return angle_radians / pi<T> * T{ 180 };
}

// Modulo operator which supports wrapping negative numbers.
// e.g. Mod(-1, 2) returns 1.
template <std::integral T>
[[nodiscard]] T Mod(T a, T b) {
	return (a % b + b) % b;
}

// Angle in degrees from [0, 360).
template <Arithmetic T>
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
template <std::floating_point T>
[[nodiscard]] T ClampAngle2Pi(T angle_radians) {
	T clamped{ std::fmod(angle_radians, two_pi<T>) };

	if (clamped < T{ 0 }) {
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
[[nodiscard]] T Floor(T value) {
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
[[nodiscard]] T Round(T value) {
	if constexpr (std::is_floating_point_v<T>) {
		return Floor(value + 0.5f);
	} else {
		return value;
	}
}

// No NaN/inf checking.
template <typename T>
[[nodiscard]] T Ceil(T value) {
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
[[nodiscard]] T Abs(T value) noexcept {
	return value >= 0 ? value : -value;
}

template <typename T>
[[nodiscard]] T Min(T a, T b) {
	return a < b ? a : b;
}

template <typename T>
[[nodiscard]] T Max(T a, T b) {
	return a > b ? a : b;
}

// Returns true if there is a real solution followed by both roots
// (equal if repeated), false and roots of 0 if imaginary.
template <std::floating_point T>
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

// Triangle wave mimicking the typical sine wave. y values in range [-1, 1], x values in domain [0,
// 1]. Starts from y=0 going toward y=1.
template <std::floating_point T>
[[nodiscard]] T TriangleWave(
	T t, T period = static_cast<T>(1.0), T phase_shift = static_cast<T>(0.0)
) {
	PTGN_ASSERT(period != static_cast<T>(0.0), "Triangle wave period can not be 0");

	t += phase_shift + static_cast<T>(0.25);
	t /= period;

	return static_cast<T>(2.0) * Abs(static_cast<T>(2.0) * (t - Floor(t + static_cast<T>(0.5)))) -
		   static_cast<T>(1.0);
}

template <Arithmetic T, std::floating_point U>
[[nodiscard]] U Lerp(T a, T b, U t) {
	return a + t * (b - a);
}

template <Arithmetic T, std::floating_point U>
[[nodiscard]] U CosineInterpolate(T a, T b, U t) {
	return Lerp(a, b, static_cast<U>(0.5) * (static_cast<U>(1) - std::cos(t * pi<U>)));
}

// From https://paulbourke.net/miscellaneous/interpolation/
template <Arithmetic T, std::floating_point U>
[[nodiscard]] U CubicInterpolate(T y0, T y1, T y2, T y3, U t) {
	U mu2 = t * t;
	U a0  = y3 - y2 - y0 + y1;
	U a1  = y0 - y1 - a0;
	U a2  = y2 - y0;
	U a3  = y1;
	return (a0 * t * mu2 + a1 * mu2 + a2 * t + a3);
}

template <std::floating_point U>
[[nodiscard]] U Quintic(U t) {
	return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

template <std::floating_point U>
[[nodiscard]] U QuinticInterpolate(U a, U b, U t) {
	return Lerp(a, b, Quintic(t));
}

template <std::floating_point U>
[[nodiscard]] U Smoothstep(U t) {
	return t * t * (3.0f - 2.0f * t);
}

// From: https://en.wikipedia.org/wiki/Smoothstep
template <std::floating_point U>
[[nodiscard]] U SmoothstepInterpolate(U a, U b, U t) {
	return Lerp(a, b, Smoothstep(t));
}

} // namespace ptgn