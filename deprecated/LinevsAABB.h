#pragma once

#include <algorithm> // std::swap
#include <cmath> // std::isnan
#include <tuple> // std::pair

#include "math/Vector2.h"
#include "physics/shapes/AABB.h"
#include "physics/Manifold.h"

namespace ptgn {

namespace math {

// Return collision manifold between line and an AABB.
inline std::pair<double, Manifold> IntersectionLinevsAABB(const V2_double& line_origin, 
														  const V2_double& line_direction, 
														  const AABB& shape,
														  const V2_double& position) {

	Manifold manifold;

	// Cache division.
	const V2_double inverse_direction{ 1.0 / line_direction };

	// Calculate intersections with rectangle bounding axes.
	V2_double t_near{ (position - line_origin) * inverse_direction };
	V2_double t_far{ (position + shape.size - line_origin) * inverse_direction };

	// Discard 0 / 0 divisions.
	if (std::isnan(t_far.y) || std::isnan(t_far.x)) {
		return { 1.0, manifold };
	}
	if (std::isnan(t_near.y) || std::isnan(t_near.x)) {
		return { 1.0, manifold };
	}

	// Sort axis collision times so t_near contains the shorter time.
	if (t_near.x > t_far.x) {
		std::swap(t_near.x, t_far.x);
	}
	if (t_near.y > t_far.y) {
		std::swap(t_near.y, t_far.y);
	}

	// Early rejection.
	if (t_near.x > t_far.y || t_near.y > t_far.x) return { 1.0, manifold };

	// Closest time will be the first contact.
	auto t_hit_near{ std::max(t_near.x, t_near.y) };

	// Furthest time is contact on opposite side of target.
	auto t_hit_far{ std::min(t_far.x, t_far.y) };

	// Reject if furthest time is negative, meaning the object is travelling away from the target.
	if (t_hit_far < 0.0) {
		return { 1.0, manifold };
	}

	// Contact point of collision from parametric line equation.
	manifold.contact_point = line_origin + line_direction * t_hit_near;

	// Find which axis collides further along the movement time.
	if (t_near.x > t_near.y) { // X-axis.
		// Direction of movement.
		if (inverse_direction.x < 0.0) {
			manifold.normal = { 1.0, 0.0 };
		} else {
			manifold.normal = { -1.0, 0.0 };
		}
	} else if (t_near.x < t_near.y) { // Y-axis.
		// Direction of movement.
		if (inverse_direction.y < 0.0) {
			manifold.normal = { 0.0, 1.0 };
		} else {
			manifold.normal = { 0.0, -1.0 };
		}
	} else if (t_near.x == t_near.y && t_far.x == t_far.y) { // Both axes collide at the same time.
		// Diagonal collision, set normal to opposite of direction of movement.
		manifold.normal = line_direction.Identity().Opposite();
	}

	manifold.penetration = line_direction * (t_hit_far - t_hit_near) * manifold.normal;

	// Raycast collision occurred.
	return { t_hit_near, manifold };
}

// Check if a line collides with an AABB.
inline bool LinevsAABB(const V2_double& line_origin,
					   const V2_double& line_direction,
					   const AABB& shape,
					   const V2_double& position) {
	return IntersectionLinevsAABB(line_origin, line_direction, shape, position).second.CollisionOccured();
}

} // namespace math

} // namespace ptgn