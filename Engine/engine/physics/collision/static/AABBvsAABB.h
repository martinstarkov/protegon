#pragma once

#include "renderer/AABB.h"

#include "physics/collision/CollisionManifold.h"

namespace engine {

namespace collision {

// Check if two AABBs overlap.
static bool AABBvsAABB(const AABB& A, const AABB& B) {
	// If any side of the aabb it outside the other aabb, there cannot be an overlap.
	if (A.position.x + A.size.x <= B.position.x || A.position.x >= B.position.x + B.size.x) return false;
	if (A.position.y + A.size.y <= B.position.y || A.position.y >= B.position.y + B.size.y) return false;
	return true;
}

// Find the penetration vector of one AABB into another AABB.
// A is the box you want the penetration to be for.
// B is the box with which you are overlapping.
static V2_double IntersectionAABBvsAABB(const AABB& A, const AABB& B) {
	V2_double penetration = { 0.0, 0.0 };
	const auto dx = B.position.x - A.position.x;
	const auto A_half = A.size / 2.0;
	const auto B_half = B.size / 2.0;
	const auto px = (B_half.x + A_half.x) - math::FastAbs(dx);
	if (px <= 0) {
		return penetration;
	}
	const auto dy = B.position.y - A.position.y;
	const auto py = (B_half.y + A_half.y) - math::FastAbs(dy);
	if (py <= 0) {
		return penetration;
	}
	if (px < py) {
		const auto sx = math::Sign(dx);
		penetration.x = px * sx;
		//normal.x = sx;
		//pos.x = A.position.x + (A_half.x * sx);
		//pos.y = B.position.y;
	} else {
		const auto sy = math::Sign(dy);
		penetration.y = py * sy;
		//normal.y = sy;
		//pos.x = B.position.x;
		//pos.y = A.position.y + (A_half.y * sy);
	}
	return penetration;
}


} // namespace collision

} // namespace engine