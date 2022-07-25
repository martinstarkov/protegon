#pragma once

#include "math/Vector2.h"

// Source: http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
// Page 79.

namespace ptgn {

namespace collision {

namespace overlap {

// Check if two AABBs overlap.
// AABB positions are taken from top left.
// AABB sizes are the full extent from top left to bottom right.
template <typename T>
inline bool AABBvsAABB(const math::Vector2<T>& position,
					   const math::Vector2<T>& size, 
					   const math::Vector2<T>& other_position,
					   const math::Vector2<T>& other_size) {
	if (position.x + size.x < other_position.x || position.x > other_position.x + other_size.x) return false;
	if (position.y + size.y < other_position.y || position.y > other_position.y + other_size.y) return false;
	return true;
}

} // namespace overlap

} // namespace collision

} // namespace ptgn