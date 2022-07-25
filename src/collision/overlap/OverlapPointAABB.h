#pragma once

#include "math/Vector2.h"

namespace ptgn {

namespace collision {

namespace overlap {

// Check if a point an AABB overlap.
// AABB position is taken from top left.
// AABB size is the full extent from top left to bottom right.
template <typename T>
inline bool PointvsAABB(const math::Vector2<T>& point,
						const math::Vector2<T>& aabb_position,
						const math::Vector2<T>& aabb_size) {
	if (point.x < aabb_position.x || point.x > aabb_position.x + aabb_size.x) return false;
	if (point.y < aabb_position.y || point.y > aabb_position.y + aabb_size.y) return false;
	return true;
}

} // namespace overlap

} // namespace collision

} // namespace ptgn