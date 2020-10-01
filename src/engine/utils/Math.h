#pragma once

#include "Vector2.h"
#include <cmath>

namespace engine {

namespace math {

template <typename T>
inline T Distance(const Vector2<T>& lhs, const Vector2<T>& rhs) {
	return static_cast<T>(std::sqrt((lhs.x - rhs.x) * (lhs.x - rhs.x) + (lhs.y - rhs.y) * (lhs.y - rhs.y)));
}

} // namespace math

} // namespace engine