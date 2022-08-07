#pragma once

#include "math/Vector2.h"
#include "math/Math.h"

// Source: http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
// Page 88.

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
	const math::Vector2<T> distance{ position - other_position };
	const T distance_squared{ distance.Dot(distance) };
	const T combined_radius{ radius + other_radius };
	const T combined_squared{ combined_radius * combined_radius };
	return distance_squared < combined_squared || math::Compare(distance_squared, combined_squared);
}

} // namespace overlap

} // namespace collision

} // namespace ptgn