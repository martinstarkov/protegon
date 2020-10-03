#pragma once

#include <cmath>
#include <type_traits>

namespace engine {

namespace math {

// Find the sign of a numeric type
template <typename T>
inline int sgn(T val) {
	static_assert(std::is_arithmetic<T>::value, "sgn can only accept numeric types");
	return (T(0) < val) - (val < T(0));
}

} // namespace math

} // namespace engine