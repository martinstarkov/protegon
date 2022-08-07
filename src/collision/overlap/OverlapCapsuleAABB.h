#pragma once

#include <array> // std::array
#include <limits> // std::numeric_limits
#include <tuple> // std::pair

#include "math/Vector2.h"
#include "math/Math.h"
#include "collision/overlap/OverlapCapsuleCapsule.h"
#include "collision/overlap/OverlapCircleAABB.h"

// TODO: First do 4 ClosestPointLineLine computations for each AABB edge (from OverlapCapsuleCapsule).
// Pick the minimum distance point, put a circle at that point and then do a CirclevsAABB test.

// Source: https://www.jeffreythompson.org/collision-detection/point-circle.php
// Source (used): https://doubleroot.in/lessons/circle/position-of-a-point/#:~:text=If%20the%20distance%20is%20greater,As%20simple%20as%20that!

namespace ptgn {

namespace collision {

namespace overlap {

// Check if a capsule and an AABB overlap.
// Capsule origin and destination are taken from the edge circle centers.
template <typename T, typename S = double,
	std::enable_if_t<std::is_floating_point_v<S>, bool> = true>
static bool CapsulevsAABB(const math::Vector2<T>& capsule_origin,
							  const math::Vector2<T>& capsule_destination,
							  const T capsule_radius,
							  const math::Vector2<T>& aabb_position,
							  const math::Vector2<T>& aabb_size) {
	static_assert(!tt::is_narrowing_v<T, S>);
	using Edge = std::pair<math::Vector2<T>, math::Vector2<T>>;
	math::Vector2<T> top_right{ aabb_position.x + aabb_size.x, aabb_position.y };
	math::Vector2<T> bottom_right{ aabb_position + aabb_size };
	math::Vector2<T> bottom_left{ aabb_position.x, aabb_position.y + aabb_size.y };
	std::array<Edge, 4> edges;
	edges.at(0) = { aabb_position, top_right };
	edges.at(1) = { top_right, bottom_right };
	edges.at(2) = { bottom_right, bottom_left };
	edges.at(3) = { bottom_left, aabb_position };
	S minimum_distance{ std::numeric_limits<S>::infinity() };
	math::Vector2<S> minimum_capsule_point;
	//math::Vector2<T> minimum_edge_point;
	// Find shortest distance between capsule line and AABB by iterating over each edge of the AABB.
	for (auto& [origin, destination] : edges) {
		S s{};
		S t{};
		math::Vector2<S> c1;
		math::Vector2<S> c2;
		const S distance_squared{ math::ClosestPointLineLine<S>(capsule_origin,
																capsule_destination,
														        origin,
																destination,
														        s, t, c1, c2) };
		if (distance_squared < minimum_distance) {
			minimum_distance = distance_squared;
			// Point on the capsule that was the closest.
			minimum_capsule_point = c1;
			//minimum_edge_point = c2;
		}
	}
	// Simply check if the closest point on the capsule (as a circle) overlaps with the AABB.
	return CirclevsAABB(minimum_capsule_point,
						static_cast<S>(capsule_radius), 
						static_cast<math::Vector2<S>>(aabb_position),
						static_cast<math::Vector2<S>>(aabb_size));
}

} // namespace overlap

} // namespace collision

} // namespace ptgn