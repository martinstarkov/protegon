#pragma once

#include "math/Vector2.h"

namespace ptgn {

namespace math {

template <typename T>
T SquareDistancePointAABB(const Vector2<T>& point, const Vector2<T>& position, const Vector2<T>& size) {
	T square_distance{ 0 };
	Vector2<T> max{ position + size };
	for (std::size_t i{ 0 }; i < 2; ++i) {
		T v{ point[i] };
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
	T square_distance{ math::SquareDistancePointAABB(circle_position, aabb_position, aabb_size) };
	// TODO: Check if this should be an epsilon comparison for floating points.
	return square_distance <= circle_radius * circle_radius;
}

} // namespace overlap

} // namespace collision

} // namespace ptgn