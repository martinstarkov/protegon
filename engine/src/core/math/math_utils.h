#pragma once

#include <algorithm>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <cstdlib>
#include <numbers>
#include <tuple>
#include <type_traits>
#include <utility>

#include "core/assert.h"
#include "core/util/concepts.h"

namespace ptgn {

inline constexpr float kPi{ std::numbers::pi_v<float> };

inline constexpr float kTwoPi{ 2.0f * kPi };

inline constexpr float kHalfPi{ kPi / 2.0f };

inline constexpr float kSqrtTwo{ std::numbers::sqrt2_v<float> };

inline constexpr float kEuler{ std::numbers::e_v<float> };

/// @brief Clamp a value between 0 and 1.
template <Arithmetic T>
[[nodiscard]] constexpr T Clamp01(T value) {
	return std::clamp(value, T{ 0 }, T{ 1 });
}

/// @brief Modulo operator which supports wrapping negative numbers.
/// e.g. Mod(-1, 2) returns 1.
template <std::integral T>
[[nodiscard]] constexpr T Mod(T a, T b) {
	return (a % b + b) % b;
}

/// @brief Signum function.
/// Returns  1  if value is positive.
/// Returns  0  if value is zero.
/// Returns -1  if value is negative.
/// No NaN/inf checking.
template <Arithmetic T>
[[nodiscard]] constexpr T Sign(T value) {
	return static_cast<T>((0 < value) - (value < 0));
}

/// @return Integer value wrapped to mod n in positive and negative directions.
[[nodiscard]] constexpr int ModFloor(int a, int n) {
	return ((a % n) + n) % n;
}

/// @brief Fast floor function (same as std::floor but without NaN/inf checking).
/// From: https://stackoverflow.com/a/30308919
template <Arithmetic T>
[[nodiscard]] constexpr T Floor(T value) {
	if constexpr (std::is_floating_point_v<T>) {
		auto truncated{ static_cast<std::int64_t>(value) };

		return static_cast<T>(
			truncated - static_cast<std::int64_t>(value < static_cast<T>(truncated))
		);
	} else {
		return value;
	}
}

/// @brief Fast round function (same as std::round but without NaN/inf checking).
template <Arithmetic T>
[[nodiscard]] constexpr T Round(T value) {
	if constexpr (std::is_floating_point_v<T>) {
		return Floor(value + 0.5f);
	} else {
		return value;
	}
}

/// @brief Fast ceil function (same as std::ceil but without NaN/inf checking).
template <Arithmetic T>
[[nodiscard]] constexpr T Ceil(T value) {
	if constexpr (std::is_floating_point_v<T>) {
		auto truncated{ static_cast<std::int64_t>(value) };

		return static_cast<T>(
			truncated + static_cast<std::int64_t>(value > static_cast<T>(truncated))
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
[[nodiscard]] constexpr float TriangleWave(float t, float period = 1.0f, float phase_shift = 0.0f) {
	PTGN_ASSERT(period != 0.0f, "Triangle wave period can not be 0");

	t += phase_shift + 0.25f;
	t /= period;

	return 2.0f * std::abs(2.0f * (t - Round(t))) - 1.0f;
}

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

[[nodiscard]] constexpr float Quintic(float t) {
	return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

/// @brief Quintic interpolate between a and b by t.
[[nodiscard]] constexpr float QuinticInterpolate(float a, float b, float t) {
	return Lerp(a, b, Quintic(t));
}

[[nodiscard]] constexpr float Smoothstep(float t) {
	return t * t * (3.0f - 2.0f * t);
}

/// @brief Smoothstep interpolate between a and b by t.
/// From: https://en.wikipedia.org/wiki/Smoothstep
[[nodiscard]] constexpr float SmoothstepInterpolate(float a, float b, float t) {
	/// From: https://en.wikipedia.org/wiki/Smoothstep
	return Lerp(a, b, Smoothstep(t));
}

} // namespace ptgn