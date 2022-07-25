#pragma once

#include "math/Vector2.h"

// Source: https://www.jeffreythompson.org/collision-detection/point-circle.php
// Source: https://doubleroot.in/lessons/circle/position-of-a-point/#:~:text=If%20the%20distance%20is%20greater,As%20simple%20as%20that!

namespace ptgn {

namespace collision {

namespace overlap {

// Check if a point and a circle overlap.
// Circle position is taken from its center.
template <typename T>
inline bool PointvsCircle(const math::Vector2<T>& point,
						  const math::Vector2<T>& circle_position,
						  const T circle_radius) {
	const math::Vector2<T> distance{ point - circle_position };
	const T distance_squared{ distance.DotProduct(distance) };
	const T radius_squared{ circle_radius * circle_radius };
	return distance_squared < radius_squared || math::Compare(distance_squared, radius_squared);
}

} // namespace overlap

} // namespace collision

} // namespace ptgn