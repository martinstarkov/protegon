#pragma once

#include "math/Vector2.h"
#include "physics/Transform.h"
#include "physics/Manifold.h"
#include "physics/shapes/AABB.h"

namespace engine {

namespace math {

// Determine if a point lies inside an AABB.
inline bool PointvsAABB(const V2_double& point, 
						const AABB& shape,
						const V2_double& position) {
	return (point.x >= position.x &&
			point.y >= position.y &&
			point.x < position.x + shape.size.x &&
			point.y < position.y + shape.size.y);
}

// Return the intersection details of a point with an AABB.
inline Manifold IntersectionPointvsAABB(const V2_double& point,
										const AABB& shape,
										const V2_double& position) {
	Manifold manifold;
	const auto dx{ point.x - position.x };
	const auto half{ shape.size / 2.0 };
	const auto px{ half.x - math::Abs(dx) };
	if (px <= 0) {
		return manifold;
	}
	const auto dy{ point.y - position.y };
	const auto py{ half.y - math::Abs(dy) };
	if (py <= 0) {
		return manifold;
	}
	if (px < py) {
		const auto sx{ math::Sign(dx) };
		manifold.penetration.x = px * sx;
		manifold.normal.x = sx;
		manifold.contact_point.y = point.y;
		manifold.contact_point.x = position.x + half.x * sx;
	} else {
		const auto sy{ math::Sign(dy) };
		manifold.penetration.y = py * sy;
		manifold.normal.y = sy;
		manifold.contact_point.x = point.x;
		manifold.contact_point.y = position.y + half.y * sy;
	}
	return manifold;
}

} // namespace math

} // namespace engine