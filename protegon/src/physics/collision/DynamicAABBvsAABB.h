#pragma once

#include <tuple> // std::pair

#include "physics/shapes/AABB.h"
#include "physics/collision/Collision.h"
#include "physics/collision/LinevsAABB.h"

namespace engine {

namespace math {

// Determine the time at which a dynamic AABB would collide with a static AABB.
// Pair contains collision time.
static std::pair<double, Manifold> DynamicAABBvsAABB(const AABB& dynamic_shape,
													 const V2_double& dynamic_position,
													 const V2_double& dynamic_velocity,
													 const AABB& static_shape,
													 const V2_double& static_position) {
	Manifold manifold;

	// Check if dynamic object has a non-zero velocity. It cannot collide if it is not moving.
	if (dynamic_velocity.IsZero()) {
		return { 1.0, manifold };
	}

	V2_double dynamic_half{ dynamic_shape.size / 2.0 };
	// Expand static target by dynamic object dimensions so that 
	// only the center of the dynamic object needstobe considered.
	V2_double relative_position{ static_position - dynamic_half };
	AABB combined_shape{ static_shape.size + dynamic_shape.size };

	V2_double dynamic_center{ dynamic_position + dynamic_half };
	// Check if the velocity ray collides with the expanded target.
	auto [nearest_time, m] = IntersectionLinevsAABB(dynamic_center, 
													dynamic_velocity, 
													combined_shape, 
													relative_position);

	manifold = m;

	if (!manifold.normal.IsZero() && nearest_time >= 0.0 && nearest_time < 1.0) {
		return { nearest_time, manifold };
	} else {
		manifold.normal = {};
		manifold.penetration = {};
		manifold.contact_point = {};
		return { 1.0, manifold };
	}
}

// Modify the velocity of a dynamic AABB so it does not collide with a static AABB.
static std::pair<double, Manifold> ResolveDynamicAABBvsAABB(const AABB& dynamic_shape,
															const V2_double& dynamic_position,
															V2_double& dynamic_velocity,
															const AABB& static_shape,
															const V2_double& static_position) {
	auto [nearest_time, manifold] = DynamicAABBvsAABB(dynamic_shape, 
											  dynamic_position,
											  dynamic_velocity,
											  static_shape,
											  static_position);
	// Repeat check is needed due to the fact that if multiple collisions are found, resolving the velocityforthe nearest one may invalidate the previously thought collisions.
	if (!manifold.normal.IsZero()) {
		dynamic_velocity += manifold.normal * math::Abs(dynamic_velocity) * (1.0 - nearest_time);
		return { nearest_time, manifold };
	}
	return { 1.0, manifold };
}

} // namespace math

} // namespace engine