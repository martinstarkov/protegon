#pragma once

#include <algorithm>
#include <cmath>
#include <concepts>
#include <cstdlib>
#include <limits>
#include <ranges>
#include <type_traits>
#include <utility>

#include "core/util/concepts.h"

namespace ptgn {

template <Arithmetic T>
inline constexpr T kEpsilon{ std::numeric_limits<T>::epsilon() };

[[nodiscard]] constexpr bool StrictlyLess(float a, float b, float epsilon = kEpsilon<float>) {
	return (b - a) > std::max(std::abs(a), std::abs(b)) * epsilon;
}

template <Arithmetic T>
inline constexpr T kAbsoluteTolerance{ static_cast<T>(10) * kEpsilon<T> };

template <Arithmetic T>
inline constexpr T kRelativeTolerance{ static_cast<T>(10) * kEpsilon<T> };

/// @brief Compare two floating point numbers using relative tolerance and absolute
/// tolerances. The absolute tolerance test fails when x and y become large. The
/// relative tolerance test fails when x and y become small.
/// Source: https://stackoverflow.com/a/65015333
template <Arithmetic T>
[[nodiscard]] constexpr bool NearlyEqual(
	T a, T b, T abs_tol = kAbsoluteTolerance<T>, T rel_tol = kRelativeTolerance<T>
) noexcept {
	if constexpr (std::is_floating_point_v<T>) {
		if (std::isnan(a) || std::isnan(b)) {
			return false;
		}

		if (std::isinf(a) || std::isinf(b)) {
			return std::isinf(a) && std::isinf(b) && std::signbit(a) == std::signbit(b);
		}

		T diff = std::abs(a - b);
		return a == b || diff <= std::max(abs_tol, rel_tol * std::max(std::abs(a), std::abs(b)));
	} else {
		return a == b;
	}
}

template <
	std::ranges::input_range R1, std::ranges::input_range R2,
	Arithmetic T = std::ranges::range_value_t<R1>>
	requires std::same_as<T, std::ranges::range_value_t<R2>>
[[nodiscard]] constexpr bool NearlyEqual(
	R1&& a, R2&& b, T abs_tol = kAbsoluteTolerance<T>, T rel_tol = kRelativeTolerance<T>
) {
	return std::ranges::equal(
		std::forward<R1>(a), std::forward<R2>(b),
		[abs_tol, rel_tol]<typename U, typename V>(const U& x, const V& y) {
			return NearlyEqual(x, y, abs_tol, rel_tol);
		}
	);
}

template <Arithmetic T>
[[nodiscard]] constexpr bool LessOrNearlyEqual(T a, T b) noexcept {
	return a < b || NearlyEqual(a, b);
}

template <Arithmetic T>
[[nodiscard]] constexpr bool WithinRangeInclusive(T value, T min, T max) noexcept {
	return value >= min && value <= max;
}

template <Arithmetic T>
[[nodiscard]] constexpr bool WithinRangeExclusive(T value, T min, T max) noexcept {
	return value > min && value < max;
}

template <Arithmetic T>
[[nodiscard]] constexpr bool NearlyWithinRangeInclusive(T value, T min, T max) noexcept {
	return WithinRangeInclusive(value, min, max) || NearlyEqual(value, min) ||
		   NearlyEqual(value, max);
}

} // namespace ptgn