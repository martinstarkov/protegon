#pragma once

#include "math/Vector2.h"
#include "physics/Manifold.h"
#include "physics/shapes/AABB.h"

namespace ptgn {

namespace math {

// Check if two AABBs overlap.
inline bool AABBvsAABB(const AABB& shapeA,
					   const V2_double& positionA,
					   const AABB& shapeB,
					   const V2_double& positionB) {
	// If any side of the aabb it outside the other aabb, there cannot be an overlap.
	if (positionA.x + shapeA.size.x <= positionB.x || positionA.x >= positionB.x + shapeB.size.x) return false;
	if (positionA.y + shapeA.size.y <= positionB.y || positionA.y >= positionB.y + shapeB.size.y) return false;
	return true;
}

// Find the penetration vector of one AABB into another AABB.
// shapeA is the box you want the penetration to be for.
// shapeB is the box with which you are overlapping.
inline Manifold IntersectionAABBvsAABB(const AABB& shapeA,
									   const V2_double& positionA,
									   const AABB& shapeB,
									   const V2_double& positionB) {
	Manifold manifold;
	const auto dx{ positionB.x - positionA.x };
	const auto A_half{ shapeA.size / 2.0 };
	const auto B_half{ shapeB.size / 2.0 };
	const auto px{ (B_half.x + A_half.x) - math::Abs(dx) };
	if (px <= 0) {
		return manifold;
	}
	const auto dy{ positionB.y - positionA.y };
	const auto py{ (B_half.y + A_half.y) - math::Abs(dy) };
	if (py <= 0) {
		return manifold;
	}
	if (px < py) {
		const auto sx{ math::Sign(dx) };
		manifold.penetration.x = px * sx;
		manifold.normal.x = sx;
		manifold.contact_point.x = positionA.x + (A_half.x * sx);
		manifold.contact_point.y = positionB.y;
	} else {
		const auto sy{ math::Sign(dy) };
		manifold.penetration.y = py * sy;
		manifold.normal.y = sy;
		manifold.contact_point.x = positionB.x;
		manifold.contact_point.y = positionA.y + (A_half.y * sy);
	}
	return manifold;
}

} // namespace math

} // namespace ptgn