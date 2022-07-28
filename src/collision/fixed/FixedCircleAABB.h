#pragma once

#include <tuple> // std::pair

// Source: https://steamcdn-a.akamaihd.net/apps/valve/2015/DirkGregorius_Contacts.pdf

namespace ptgn {

namespace math {

// Static rectangle and circle collision detection.
inline Manifold IntersectionCirclevsAABB(const Circle& shapeA,
										 const V2_double& positionA,
										 const AABB& shapeB,
										 const V2_double& positionB) {
	Manifold manifold;
	// Vector from A to B.
	const V2_double half{ shapeB.size / 2.0 };
	const V2_double n{ positionA - (positionB + half) };
	// Closest point on A to center of B.
	V2_double closest{ n };
	// Clamp point to edges of the AABB.
	closest = math::Clamp(closest, -half, half);
	bool inside{ false };
	// Circle is inside the AABB, so we need to clamp the circle's center
	// to the closest edge
	if (n == closest) {
		inside = true;
		// Find closest axis
		if (math::Abs(n.x) > math::Abs(n.y)) {
			// Clamp to closest extent
			if (closest.x > 0.0) {
				closest.x = half.x;
			} else {
				closest.x = -half.x;
			}
		} else { // y axis is shorter
			// Clamp to closest extent
			if (closest.y > 0.0) {
				closest.y = half.y;
			} else {
				closest.y = -half.y;
			}
		}
	}

	const auto normal{ n - closest };
	const auto distance_squared{ normal.MagnitudeSquared() };
	// Early out of the radius is shorter than distance to closest point and
	// Circle not inside the AABB
	if (distance_squared > shapeA.radius * shapeA.radius && !inside) {
		return manifold;
	}
	// Avoid sqrt until we needed to take it.
	auto distance{ math::Sqrt(distance_squared) };
	// Collision normal needs to be flipped to point outside if circle was
	// inside the AABB
	if (inside) {
		manifold.normal = -n;
	} else {
		manifold.normal = n;
	}
	manifold.penetration = manifold.normal * (shapeA.radius - distance);
	manifold.contact_point = positionA + manifold.penetration;
	return manifold;
}

} // namespace math

} // namespace ptgn