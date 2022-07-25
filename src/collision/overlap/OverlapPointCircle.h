#pragma once

#include "math/Vector2.h"

namespace ptgn {

namespace collision {

namespace overlap {

// Check if a point and a circle overlap.
// Circle position is taken from its center.
template <typename T>
inline bool PointvsCircle(const math::Vector2<T>& point,
						  const math::Vector2<T>& circle_position,
						  const T circle_radius) {
	math::Vector2<T> distance{ point - circle_position };
	T distance_squared{ distance.DotProduct(distance) };
	// TODO: Check if this should be an epsilon comparison for floating points.
	return distance_squared <= circle_radius;
}

} // namespace overlap

} // namespace collision

} // namespace ptgn