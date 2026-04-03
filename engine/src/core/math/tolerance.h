#pragma once

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <type_traits>

namespace ptgn {

template <typename T>
inline constexpr T kEpsilon{ std::numeric_limits<T>::epsilon() };

[[nodiscard]] inline bool StrictlyLess(float a, float b, float epsilon = kEpsilon<float>) {
	return (b - a) > std::max(std::abs(a), std::abs(b)) * epsilon;
}

/// @brief Compare two floating point numbers using relative tolerance and absolute
/// tolerances. The absolute tolerance test fails when x and y become large. The
/// relative tolerance test fails when x and y become small.
/// Source: https://stackoverflow.com/a/65015333
template <typename T>
[[nodiscard]] constexpr bool
NearlyEqual(T a, T b, T abs_tol = static_cast<T>(10) * kEpsilon<T>, T rel_tol = static_cast<T>(10) * kEpsilon<T>) noexcept {
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

} // namespace ptgn