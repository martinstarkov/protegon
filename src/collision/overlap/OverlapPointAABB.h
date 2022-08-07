#pragma once

#include "math/Vector2.h"
#include "collision/overlap/OverlapAABBAABB.h"

// Source: http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
// Modified page 79 with size of other AABB set to 0.

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
	return AABBvsAABB(point, math::Vector2<T>{ 0, 0 }, aabb_position, aabb_size);
}

} // namespace overlap

} // namespace collision

} // namespace ptgn