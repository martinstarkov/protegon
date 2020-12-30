#pragma once

#include "renderer/AABB.h"
#include "renderer/Circle.h"

#include "physics/collision/CollisionManifold.h"

namespace engine {

namespace collision {

// Static rectangle and circle collision detection.
bool AABBvsCircle(const AABB& A, const Circle& B, CollisionManifold& out_collision) {
	// Vector from A to B
	V2_double n = B.position - A.Center();
	// Closest point on A to center of B
	V2_double closest = n;
	// Clamp point to edges of the AABB
	closest.x = engine::math::Clamp(closest.x, -A.size.x / 2.0, A.size.x / 2.0);
	closest.y = engine::math::Clamp(closest.y, -A.size.y / 2.0, A.size.y / 2.0);
	bool inside = false;
	// Circle is inside the AABB, so we need to clamp the circle's center
	// to the closest edge
	if (n == closest) {
		inside = true;
		// Find closest axis
		if (abs(n.x) > abs(n.y)) {
			// Clamp to closest extent
			if (closest.x > 0.0) {
				closest.x = A.size.x / 2.0;
			} else {
				closest.x = -A.size.x / 2.0;
			}
		} else { // y axis is shorter
			// Clamp to closest extent
			if (closest.y > 0.0) {
				closest.y = A.size.y / 2.0;
			} else {
				closest.y = -A.size.y / 2.0;
			}
		}
	}

	auto normal = n - closest;
	auto distance = normal.MagnitudeSquared();
	// Early out of the radius is shorter than distance to closest point and
	// Circle not inside the AABB
	if (distance > B.radius * B.radius && !inside) {
		return false;
	}
	// Avoid sqrt until we needed to take it.
	distance = sqrt(distance);
	// Collision normal needs to be flipped to point outside if circle was
	// inside the AABB
	if (inside) {
		out_collision.normal = -n;
		out_collision.depth = B.radius - distance;
	} else {
		out_collision.normal = n;
		out_collision.depth = B.radius - distance;
	}

	return true;
}

} // namespace collision

} // namespace engine