#pragma once

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

// Check if a point and a circle overlap.
// Circle position is taken from its center.
template <typename T>
static bool CapsulevsAABB(const math::Vector2<T>& capsule_origin,
						  const math::Vector2<T>& capsule_destination,
						  const T capsule_radius,
						  const math::Vector2<T>& aabb_position,
						  const math::Vector2<T>& aabb_size) {
	return false;
}

} // namespace overlap

} // namespace collision

} // namespace ptgn