#pragma once

#include <concepts>
#include <cstdint>
#include <numbers>
#include <tuple>
#include <type_traits>

#include "core/util/concepts.h"

namespace ptgn {

inline constexpr float kPi{ std::numbers::pi_v<float> };

inline constexpr float kTwoPi{ 2.0f * kPi };

inline constexpr float kHalfPi{ kPi / 2.0f };

inline constexpr float kSqrtTwo{ std::numbers::sqrt2_v<float> };

inline constexpr float kEuler{ std::numbers::e_v<float> };

/// @brief Convert degrees to radians.
[[nodiscard]] constexpr float DegToRad(float angle_degrees) {
	return angle_degrees * kPi / 180.0f;
}

/// @brief Convert radians to degrees.
[[nodiscard]] constexpr float RadToDeg(float angle_radians) {
	return angle_radians / kPi * 180.0f;
}

/// @brief Modulo operator which supports wrapping negative numbers.
/// e.g. Mod(-1, 2) returns 1.
template <std::integral T>
[[nodiscard]] T Mod(T a, T b) {
	return (a % b + b) % b;
}

/// @brief Angle in degrees from [0, 360).
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

/// @return Angle in radians in range [0, 2 pi).
[[nodiscard]] float ClampAngle2Pi(float angle_radians);

/// @brief Signum function.
/// Returns  1  if value is positive.
/// Returns  0  if value is zero.
/// Returns -1  if value is negative.
/// No NaN/inf checking.
template <typename T>
[[nodiscard]] constexpr T Sign(T value) {
	return static_cast<T>((0 < value) - (value < 0));
}

/// @return Integer value wrapped to mod n in positive and negative directions.
[[nodiscard]] constexpr int ModFloor(int a, int n) {
	return ((a % n) + n) % n;
}

/// @brief Fast floor function (same as std::floor but without NaN/inf checking).
/// From: https://stackoverflow.com/a/30308919
template <typename T>
[[nodiscard]] constexpr T FastFloor(T value) {
	if constexpr (std::is_floating_point_v<T>) {
		return static_cast<T>(
			static_cast<std::int64_t>(value) - (value < static_cast<std::int64_t>(value))
		);
	} else {
		return value;
	}
}

/// @brief Fast round function (same as std::round but without NaN/inf checking).
template <typename T>
[[nodiscard]] constexpr T FastRound(T value) {
	if constexpr (std::is_floating_point_v<T>) {
		return FastFloor(value + 0.5f);
	} else {
		return value;
	}
}

/// @brief Fast ceil function (same as std::ceil but without NaN/inf checking).
template <typename T>
[[nodiscard]] constexpr T FastCeil(T value) {
	if constexpr (std::is_floating_point_v<T>) {
		return static_cast<T>(
			static_cast<std::int64_t>(value) + (value > static_cast<std::int64_t>(value))
		);
	} else {
		return value;
	}
}

/// @return True if there is a real solution followed by both roots
/// (equal if repeated), false and roots of 0 if imaginary.
[[nodiscard]] std::tuple<bool, float, float> QuadraticFormula(float a, float b, float c);

/// @brief Triangle wave mimicking the typical sine wave. y values in range [-1, 1], x values in
/// domain [0, 1]. Starts from y=0 going toward y=1.
[[nodiscard]] float TriangleWave(float t, float period = 1.0f, float phase_shift = 0.0f);

/// @brief Linearly interpolate between a and b by t.
template <Arithmetic T>
[[nodiscard]] constexpr float Lerp(T a, T b, float t) {
	return a + t * (b - a);
}

/// @brief Cosine interpolate between a and b by t.
template <Arithmetic T>
[[nodiscard]] constexpr float CosineInterpolate(T a, T b, float t) {
	return Lerp(a, b, 0.5f * (1.0f - std::cos(t * kPi)));
}

/// @brief From https://paulbourke.net/miscellaneous/interpolation/
template <Arithmetic T>
[[nodiscard]] constexpr float CubicInterpolate(T y0, T y1, T y2, T y3, float t) {
	float mu2 = t * t;
	float a0  = y3 - y2 - y0 + y1;
	float a1  = y0 - y1 - a0;
	float a2  = y2 - y0;
	float a3  = y1;
	return (a0 * t * mu2 + a1 * mu2 + a2 * t + a3);
}

[[nodiscard]] float Quintic(float t);

/// @brief Quintic interpolate between a and b by t.
[[nodiscard]] float QuinticInterpolate(float a, float b, float t);

[[nodiscard]] float Smoothstep(float t);

/// @brief Smoothstep interpolate between a and b by t.
/// From: https://en.wikipedia.org/wiki/Smoothstep
[[nodiscard]] float SmoothstepInterpolate(float a, float b, float t);

} // namespace ptgn