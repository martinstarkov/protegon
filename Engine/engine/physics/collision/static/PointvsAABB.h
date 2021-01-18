#pragma once

#include "utils/Vector2.h"
#include "renderer/AABB.h"

namespace engine {

namespace collision {

// Determine if a point lies inside an AABB.
static bool PointvsAABB(const V2_double& point, const AABB& A) {
	return (point.x >= A.position.x &&
			point.y >= A.position.y &&
			point.x < A.position.x + A.size.x &&
			point.y < A.position.y + A.size.y);
}

static V2_double IntersectionPointvsAABB(const V2_double& point, const AABB& A) {
	V2_double penetration = { 0.0, 0.0 };
	const auto dx = point.x - A.position.x;
	const auto half = A.size / 2.0;
	const auto px = half.x - math::FastAbs(dx);
	if (px <= 0) {
		return penetration;
	}
	const auto dy = point.y - A.position.y;
	const auto py = half.y - math::FastAbs(dy);
	if (py <= 0) {
		return penetration;
	}
	if (px < py) {
		const auto sx = math::Sign(dx);
		penetration.x = px * sx;
		//normal.x = sx;
		//pos.y = point.y;
		//pos.x = A.position.x + (half.x * sx);
	} else {
		const auto sy = math::Sign(dy);
		penetration.y = py * sy;
		//normal.y = sy;
		//pos.x = point.x;
		//pos.y = A.position.y + (half.y * sy);
	}
	return penetration;
}


} // namespace collision

} // namespace engine