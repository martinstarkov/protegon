#pragma once

#include "math/Vector2.h"
#include "math/Math.h"
#include "collision/overlap/OverlapCircleCircle.h"

// Source: http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
// Page 114; page 130 for PointToLineSquareDistance function.

namespace ptgn {

namespace math {

// Source: file:///C:/Users/Martin/Desktop/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
// Page 130.
// Returns the squared distance between point and segment line_origin -> line_destination.
template <typename T, typename S = double,
	std::enable_if_t<std::is_floating_point_v<S>, bool> = true>
const S PointToLineSquareDistance(const math::Vector2<T>& point, 
								  const math::Vector2<T>& line_origin, 
								  const math::Vector2<T>& line_destination) {
	const math::Vector2<S> ab = line_destination - line_origin;
	const math::Vector2<S> ac = point - line_origin;
	const math::Vector2<S> bc = point - line_destination;
	const S e{ ac.Dot(ab) };
	// Handle cases where c projects outside ab
	if (e < 0 || math::Compare(e, 0)) return ac.Dot(ac);
	const S f{ ab.Dot(ab) };
	if (e > f || math::Compare(e, f)) return bc.Dot(bc);
	// Handle cases where c projects onto ab
	return ac.Dot(ac) - e * e / f;
}

// Source: file:///C:/Users/Martin/Desktop/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
// Page 129.
// Given segment ab and point c, computes closest point out_d on ab.
// Also returns out_t for the position of out_d, out_d(out_t)= a + out_t * (b - a)
template <typename T, typename S = double,
	std::enable_if_t<std::is_floating_point_v<S>, bool> = true>
void ClosestPointLine(const math::Vector2<T>& point,
					  const math::Vector2<T>& line_origin,
					  const math::Vector2<T>& line_destination,
					  S& out_t,
					  math::Vector2<S>& out_d) {
	const math::Vector2<S> ab{ line_destination - line_origin };
	// Project c onto ab, but deferring divide by Dot(ab, ab)
	out_t = (point - line_origin).Dot(ab);
	if (out_t < 0 || math::Compare(out_t, 0)) {
		// c projects outside the [a,b] interval, on the a side; clamp to a
		out_t = 0;
		out_d = line_origin;
	} else {
		S denom = ab.Dot(ab); // Always nonnegative since denom = ||ab||^2
		if (out_t > denom || math::Compare(out_t, denom)) {
			// c projects outside the [a,b] interval, on the b side; clamp to b
			out_t = 1;
			out_d = line_destination;
		} else {
			// c projects inside the [a,b] interval; must do deferred divide now
			out_t = out_t / denom;
			out_d = line_origin + out_t * ab;
		}
	}
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
	static_assert(!tt::is_narrowing_v<T, S>);
	// Compute (squared) distance between sphere center and capsule line segment.
	const S distance_squared{ math::PointToLineSquareDistance<S>(circle_position,
																 capsule_origin,
																 capsule_destination) };
	// If (squared) distance smaller than (squared) sum of radii, they collide.
	const T combined_radius{ circle_radius + capsule_radius };
	const S combined_radius_squared{ static_cast<S>(combined_radius) * static_cast<S>(combined_radius) };
	return distance_squared < combined_radius_squared ||
		   math::Compare(distance_squared, combined_radius_squared);
}

} // namespace overlap

} // namespace collision

} // namespace ptgn