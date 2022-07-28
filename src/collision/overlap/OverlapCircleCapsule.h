#pragma once

#include "math/Vector2.h"
#include "math/Math.h"
#include "collision/overlap/OverlapCircleCircle.h"

// Source: http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
// Page 114; page 130 for PointToLineSquareDistance function.

namespace ptgn {

namespace math {

// Returns the squared distance between point c and segment ab
template <typename T, typename S = double,
	std::enable_if_t<std::is_floating_point_v<S>, bool> = true>
const S PointToLineSquareDistance(const math::Vector2<T>& point, 
								  const math::Vector2<T>& line_origin, 
								  const math::Vector2<T>& line_destination) {
	const math::Vector2<S> ab = line_destination - line_origin;
	const math::Vector2<S> ac = point - line_origin;
	const math::Vector2<S> bc = point - line_destination;
	const S e{ ac.DotProduct(ab) };
	// Handle cases where c projects outside ab
	if (e < static_cast<S>(0) || math::Compare(e, static_cast<S>(0))) return ac.DotProduct(ac);
	const S f{ ab.DotProduct(ab) };
	if (e > f || math::Compare(e, f)) return bc.DotProduct(bc);
	// Handle cases where c projects onto ab
	return ac.DotProduct(ac) - e * e / f;
}

} // namespace math

namespace collision {

namespace overlap {

// Check if a circle and a capsule overlap.
// Capsule origin and destination are taken from the edge circle centers.
template <typename T, typename S = double,
	std::enable_if_t<std::is_floating_point_v<S>, bool> = true>
static bool CirclevsCapsule(const math::Vector2<T>& circle_position,
							const T circle_radius,
							const math::Vector2<T>& capsule_origin,
							const math::Vector2<T>& capsule_destination,
							const T capsule_radius) {
	// Compute (squared) distance between sphere center and capsule line segment.
	const S distance_squared{ PointToLineSquareDistance<S>(circle_position, capsule_origin, capsule_destination) };
	// If (squared) distance smaller than (squared) sum of radii, they collide.
	const S combined_radius = circle_radius + capsule_radius;
	const S combined_radius_squared{ combined_radius * combined_radius };
	return distance_squared < combined_radius_squared ||
		   math::Compare(distance_squared, combined_radius_squared);
}

} // namespace overlap

} // namespace collision

} // namespace ptgn