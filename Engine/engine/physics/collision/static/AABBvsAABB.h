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
static V2_double IntersectionAABBvsAABB(const AABB& A, const AABB& B) {
	V2_double penetration;
	double dx = B.position.x - A.position.x;
	auto a_center = B.size / 2.0;
	auto b_center = A.size / 2.0;
	double px = (a_center.x + b_center.x) - std::abs(dx);
	if (px <= 0.0) {
		return penetration;
	}
	double dy = B.position.y - A.position.y;
	double py = (a_center.y + b_center.y) - std::abs(dy);
	if (py <= 0.0) {
		return penetration;
	}
	if (px < py) {
		double sx = engine::math::Sign(dx);
		penetration.x = px * sx;
	} else {
		double sy = engine::math::Sign(dy);
		penetration.y = py * sy;
	}
	return penetration;
}

} // namespace collision

} // namespace engine