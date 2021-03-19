#pragma once

#include "renderer/AABB.h"

#include "physics/Collision.h"

namespace engine {

namespace collision {

// Check if a line collides with an AABB.
static bool LinevsAABB(const V2_double& ray_origin, const V2_double& ray_dir, const AABB& target, CollisionManifold& out_collision) {

	// Initial condition: reset collision information.
	out_collision.normal = { 0.0, 0.0 };
	out_collision.point = { 0.0, 0.0 };

	// Cache division.
	auto inv_dir = 1.0 / ray_dir;

	// Calculate intersections with rectangle bounding axes.
	auto t_near = (target.position - ray_origin) * inv_dir;
	auto t_far = (target.position + target.size - ray_origin) * inv_dir;

	// Discard 0 / 0 divisions.
	if (std::isnan(t_far.y) || std::isnan(t_far.x)) return false;
	if (std::isnan(t_near.y) || std::isnan(t_near.x)) return false;

	// Sort axis collision times so t_near contains the shorter time.
	if (t_near.x > t_far.x) std::swap(t_near.x, t_far.x);
	if (t_near.y > t_far.y) std::swap(t_near.y, t_far.y);

	// Early rejection.
	if (t_near.x > t_far.y || t_near.y > t_far.x) return false;

	// Closest time will be the first contact.
	out_collision.time = std::max(t_near.x, t_near.y);

	// Furthest time is contact on opposite side of target.
	double t_hit_far = std::min(t_far.x, t_far.y);

	// Reject if furthest time is negative, meaning the object is travelling away from the target.
	if (t_hit_far < 0.0) {
		return false;
	}

	// Contact point of collision from parametric line equation.
	out_collision.point = ray_origin + out_collision.time * ray_dir;

	// Find which axis collides further along the movement time.
	if (t_near.x > t_near.y) { // X-axis.
		// Direction of movement.
		if (inv_dir.x < 0.0) {
			out_collision.normal = { 1.0, 0.0 };
		} else {
			out_collision.normal = { -1.0, 0.0 };
		}
	} else if (t_near.x < t_near.y) { // Y-axis.
		// Direction of movement.
		if (inv_dir.y < 0.0) {
			out_collision.normal = { 0.0, 1.0 };
		} else {
			out_collision.normal = { 0.0, -1.0 };
		}
	} else if (t_near.x == t_near.y && t_far.x == t_far.y) { // Both axes collide at the same time.
		// Diagonal collision, set normal to opposite of direction of movement.
		out_collision.normal = ray_dir.Identity().Opposite();
	}

	// Raycast collision occurred.
	return true;
}

} // namespace collision

} // namespace engine