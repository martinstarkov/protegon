#pragma once

#include "utils/Vector2.h"
#include "renderer/AABB.h"

namespace engine {

namespace collision {

// Determine if a point lies inside an AABB.
inline bool PointvsAABB(const V2_double& point, const AABB& A) {
	return (point.x >= A.position.x &&
			point.y >= A.position.y &&
			point.x < A.position.x + A.size.x &&
			point.y < A.position.y + A.size.y);
}

} // namespace collision

} // namespace engine