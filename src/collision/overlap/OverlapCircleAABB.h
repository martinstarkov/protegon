#pragma once

#include "math/Vector2.h"

// Source: http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
// Page 165-166.

namespace ptgn {

namespace math {

template <typename T>
const T SquareDistancePointAABB(const Vector2<T>& point, const Vector2<T>& position, const Vector2<T>& size) {
	T square_distance{ 0 };
	const Vector2<T> max{ position + size };
	for (std::size_t i{ 0 }; i < 2; ++i) {
		const T v{ point[i] };
		if (v < position[i]) square_distance += (position[i] - v) * (position[i] - v);
		if (v > max[i]) square_distance += (v - max[i]) * (v - max[i]);
	}
	return square_distance;
}

} // namespace math

namespace collision {

namespace overlap {

// Check if a circle and an AABB overlap.
// AABB position is taken from top left.
// AABB size is the full extent from top left to bottom right.
// Circle position is taken from its center.
template <typename T>
inline bool CirclevsAABB(const math::Vector2<T>& circle_position,
						 const T circle_radius,
						 const math::Vector2<T>& aabb_position,
						 const math::Vector2<T>& aabb_size) {
	const T square_distance{ math::SquareDistancePointAABB(circle_position, aabb_position, aabb_size) };
	const T radius_squared{ circle_radius * circle_radius };
	return square_distance < radius_squared || math::Compare(square_distance, radius_squared);
}

} // namespace overlap

} // namespace collision

} // namespace ptgn