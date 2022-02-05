#pragma once

#include "math/Vector2.h"
#include "physics/Manifold.h"
#include "physics/shapes/AABB.h"

namespace ptgn {

namespace collision {

// Check if two AABBs overlap.
inline bool AABBvsAABB(const V2_double& size_1,
					   const V2_double& top_left_1,
					   const V2_double& size_2,
					   const V2_double& top_left_2) {
	// If any side of the aabb it outside the other aabb, there cannot be an overlap.
	if (top_left_1.x + size_1.x <= top_left_2.x || top_left_1.x >= top_left_2.x + size_2.x) return false;
	if (top_left_1.y + size_1.y <= top_left_2.y || top_left_1.y >= top_left_2.y + size_2.y) return false;
	return true;
}

// Find the penetration vector of one AABB into another AABB.
// size_1 is the box you want the penetration to be for.
// size_2 is the box with which you are overlapping.
inline Manifold IntersectionAABBvsAABB(const V2_double& size_1,
									   V2_double top_left_1,
									   const V2_double& size_2,
									   V2_double top_left_2) {
	Manifold manifold;
	// Use center positions.
	const auto half_1{ size_1 / 2.0 };
	const auto half_2{ size_2 / 2.0 };
	top_left_1 += half_1;
	top_left_2 += half_2;
	const auto dx{ top_left_2.x - top_left_1.x };
	const auto px{ (half_2.x + half_1.x) - math::Abs(dx) };
	if (px <= 0) {
		return manifold;
	}
	const auto dy{ top_left_2.y - top_left_1.y };
	const auto py{ (half_2.y + half_1.y) - math::Abs(dy) };
	if (py <= 0) {
		return manifold;
	}
	if (px < py) {
		const auto sx{ math::Sign(dx) };
		manifold.penetration.x = px * sx;
		manifold.normal.x = sx;
		manifold.contact_point.x = top_left_1.x + (half_1.x * sx);
		manifold.contact_point.y = top_left_2.y;
	} else {
		const auto sy{ math::Sign(dy) };
		manifold.penetration.y = py * sy;
		manifold.normal.y = sy;
		manifold.contact_point.x = top_left_2.x;
		manifold.contact_point.y = top_left_1.y + (half_1.y * sy);
	}
	return manifold;
}

} // namespace collision

} // namespace ptgn