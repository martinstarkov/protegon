#pragma once

#include "math/Vector2.h"
#include "math/Math.h"

namespace ptgn {

namespace collision {

namespace overlap {

// Check if a line and an AABB overlap.
// AABB position is taken from top left.
// AABB size is the full extent from top left to bottom right.
template <typename T>
inline bool LinevsAABB(const math::Vector2<T>& line_origin,
					   const math::Vector2<T>& line_destination,
					   const math::Vector2<T>& aabb_position,
					   const math::Vector2<T>& aabb_size) {
	math::Vector2<T> extents{ aabb_size / 2 };
	math::Vector2<T> center{ aabb_position + extents };
	math::Vector2<T> normal{ (line_destination - line_origin).Tangent() };
	// Compute the projection interval radius of b onto L(t) = b.c + t * p.n
	T r{ extents[0] * math::Abs(normal[0]) + extents[1] * math::Abs(normal[1]) };
	// Compute distance of box center from plane
	// TODO: Check that this is correct, the second term was p.d and may not be correctly interpreted in line form.
	T s{ normal.DotProduct(center) - normal.DotProduct(line_origin) };
	// Intersection occurs when distance s falls within [-r,+r] interval
	// TODO: Check if this should be an epsilon comparison for floating points.
	return math::Abs(s) <= r;
}

} // namespace overlap

} // namespace collision

} // namespace ptgn