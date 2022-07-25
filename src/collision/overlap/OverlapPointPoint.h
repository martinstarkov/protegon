#pragma once

#include "math/Vector2.h"

namespace ptgn {

namespace collision {

namespace overlap {

// Check if two points overlap.
template <typename T>
inline bool PointvsPoint(const math::Vector2<T>& point,
						 const math::Vector2<T>& other_point) {
	return point == other_point;
}

} // namespace overlap

} // namespace collision

} // namespace ptgn