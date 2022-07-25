#pragma once

#include "math/Vector2.h"

namespace ptgn {

namespace collision {

namespace overlap {

// Check if two circles overlap.
// Circle positions are taken from their centers.
template <typename T>
inline bool CirclevsCircle(const math::Vector2<T>& position,
						   const T radius,
						   const math::Vector2<T>& other_position,
						   const T other_radius) {
	math::Vector2<T> distance{ position - other_position };
	T distance_squared{ distance.DotProduct(distance) };
	T combined_radius{ a.r + b.r };
	// TODO: Check if this should be an epsilon comparison for floating points.
	return distance_squared <= combined_radius * combined_radius;
}

} // namespace overlap

} // namespace collision

} // namespace ptgn