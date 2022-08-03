#pragma once

#include "math/Vector2.h"
#include "math/Math.h"
#include "collision/overlap/OverlapCircleCircle.h"

// Source: http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
// Pages 114-115; pages 149-150 for ClosestPointLineLine function.

namespace ptgn {

namespace math {

// Computes closest points C1 and C2 of S1(s)=P1+s*(Q1-P1) and
// S2(t)=P2+t*(Q2-P2), returning s and t. 
// Function return is squared distance between between S1(s) and S2(t)
template <typename S = double, typename T,
	std::enable_if_t<std::is_floating_point_v<S>, bool> = true>
const S ClosestPointLineLine(const math::Vector2<T>& line_origin,
					         const math::Vector2<T>& line_destination,
					         const math::Vector2<T>& other_line_origin,
					         const math::Vector2<T>& other_line_destination,
					         S& out_s,
					         S& out_t,
					         math::Vector2<S>& out_c1,
					         math::Vector2<S>& out_c2) {
	const math::Vector2<S> d1 = line_destination - line_origin; // Direction vector of segment S1
	const math::Vector2<S> d2 = other_line_destination - other_line_origin; // Direction vector of segment S2
	const math::Vector2<S> r = line_origin - other_line_origin;
	const S a = d1.DotProduct(d1); // Squared length of segment S1, always nonnegative
	const S e = d2.DotProduct(d2); // Squared length of segment S2, always nonnegative
	const S f = d2.DotProduct(r);
	// Check if either or both segments degenerate into points
	if (a <= math::EPSILON<S> && e <= math::EPSILON<S>) {
		// Both segments degenerate into points
		out_s = out_t = static_cast<S>(0);
		out_c1 = line_origin;
		out_c2 = other_line_origin;
		const math::Vector2<S> subtraction{ out_c1 - out_c2 };
		return subtraction.DotProduct(subtraction);
	}
	if (a <= math::EPSILON<S>) {
		// First segment degenerates into a point
		out_s = static_cast<S>(0);
		out_t = f / e; // out_s = 0 => out_t = (b*out_s + f) / e = f / e
		out_t = math::Clamp(out_t, static_cast<S>(0), static_cast<S>(1));
	} else {
		const S c = d1.DotProduct(r);
		if (e <= math::EPSILON<S>) {
			// Second segment degenerates into a point
			out_t = static_cast<S>(0);
			out_s = math::Clamp(-c / a, static_cast<S>(0), static_cast<S>(1)); // out_t = 0 => out_s = (b*out_t - c) / a = -c / a
		} else {
			// The general nondegenerate case starts here
			const S b = d1.DotProduct(d2);
			const S denom = a * e - b * b; // Always nonnegative
			// If segments not parallel, compute closest point on L1 to L2 and
			// clamp to segment S1. Else pick arbitrary out_s (here 0)
			if (!math::Compare(denom, static_cast<S>(0))) {
				out_s = math::Clamp((b * f - c * e) / denom, static_cast<S>(0), static_cast<S>(1));
			} else out_s = static_cast<S>(0);

			const S tnom = b * out_s + f;
			if (tnom < static_cast<S>(0)) {
				out_t = static_cast<S>(0);
				out_s = math::Clamp(-c / a, static_cast<S>(0), static_cast<S>(1));
			} else if (tnom > e) {
				out_t = static_cast<S>(1);
				out_s = math::Clamp((b - c) / a, static_cast<S>(0), static_cast<S>(1));
			} else {
				out_t = tnom / e;
			}
			/*
			// Compute point on L2 closest to S1(out_s) using
			// out_t = Dot((P1 + D1*out_s) - P2,D2) / Dot(D2,D2) = (b*out_s + f) / e
			out_t = (b * out_s + f) / e;
			// If out_t in [0,1] done. Else clamp out_t, recompute out_s for the new value
			// of out_t using out_s = Dot((P2 + D2*out_t) - P1,D1) / Dot(D1,D1)= (out_t*b - c) / a
			// and clamp out_s to [0, 1]
			if (out_t < static_cast<S>(0)) {
				out_t = static_cast<S>(0);
				out_s = Clamp(-c / a, static_cast<S>(0), static_cast<S>(1));
			} else if (out_t > static_cast<S>(1)) {
				out_t = static_cast<S>(1);
				out_s = Clamp((b - c) / a, static_cast<S>(0), static_cast<S>(1));
			}
			*/
		}
	}
	out_c1 = line_origin + d1 * out_s;
	out_c2 = other_line_origin + d2 * out_t;
	const math::Vector2<S> subtraction{ out_c1 - out_c2 };
	return subtraction.DotProduct(subtraction);
}

} // namespace math

namespace collision {

namespace overlap {

// Check if two capsules overlap.
// Capsule origins and destinations are taken from the edge circle centers.
template <typename S = double, typename T,
	std::enable_if_t<std::is_floating_point_v<S>, bool> = true>
static bool CapsulevsCapsule(const math::Vector2<T>& capsule_origin,
							 const math::Vector2<T>& capsule_destination,
							 const T capsule_radius,
							 const math::Vector2<T>& other_capsule_origin,
							 const math::Vector2<T>& other_capsule_destination,
							 const T other_capsule_radius) {
	// Compute (squared) distance between the inner structures of the capsules.
	S s;
	S t;
	math::Vector2<S> c1;
	math::Vector2<S> c2;
	const S distance_squared{ math::ClosestPointLineLine<S>(capsule_origin,
															capsule_destination,
													        other_capsule_origin,
															other_capsule_destination,
									                        s, t, c1, c2) };
	// If (squared) distance smaller than (squared) sum of radii, they collide
	const S combined_radius{ static_cast<S>(capsule_radius) + static_cast<S>(other_capsule_radius) };
	const S combined_radius_squared{ combined_radius * combined_radius };
	return distance_squared < combined_radius_squared ||
		   math::Compare(distance_squared, combined_radius_squared);
}

} // namespace overlap

} // namespace collision

} // namespace ptgn