#pragma once

namespace ptgn {

template <typename T>
inline constexpr T epsilon{ std::numeric_limits<T>::epsilon() };

// Compare two floating point numbers using relative tolerance and absolute
// tolerances. The absolute tolerance test fails when x and y become large. The
// relative tolerance test fails when x and y become small.
// Source: https://stackoverflow.com/a/65015333
template <typename T>
[[nodiscard]] constexpr bool
NearlyEqual(T a, T b, T abs_tol = static_cast<T>(10) * epsilon<T>, T rel_tol = static_cast<T>(10) * epsilon<T>) noexcept {
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