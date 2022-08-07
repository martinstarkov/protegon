#pragma once

#include <array>  // std::array
#include <limits> // std::numeric_limits
#include <tuple>  // std::pair

#include "collision/Types.h"
#include "collision/overlap/OverlapCircle.h"

namespace ptgn {

namespace math {

// Source: http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
// Pages 149-150.
// Computes closest points C1 and C2 of S1(s)=P1+s*(Q1-P1) and
// S2(t)=P2+t*(Q2-P2), returning s and t. 
// Function return is squared distance between between S1(s) and S2(t)
template <typename S = double, typename T,
	tt::floating_point<S> = true>
static S ClosestPointLineLine(const Line<T>& a,
					          const Line<T>& b,
					          S& out_s,
					          S& out_t,
					          math::Vector2<S>& out_c1,
					          math::Vector2<S>& out_c2) {
	static_assert(!tt::is_narrowing_v<T, S>);
	const math::Vector2<S> d1{ a.destination - a.origin }; // Direction vector of segment S1
	const math::Vector2<S> d2{ b.destination - b.origin }; // Direction vector of segment S2
	const math::Vector2<S> r{ a.origin - b.origin };
	const S z{ d1.Dot(d1) }; // Squared length of segment S1, always nonnegative
	const S e{ d2.Dot(d2) }; // Squared length of segment S2, always nonnegative
	const S f{ d2.Dot(r) };
	// Check if either or both segments degenerate into points
	if (z <= math::epsilon<S> && e <= math::epsilon<S>) {
		// Both segments degenerate into points
		out_s = out_t = 0;
		out_c1 = a.origin;
		out_c2 = b.origin;
		const math::Vector2<S> subtraction{ out_c1 - out_c2 };
		return subtraction.Dot(subtraction);
	}
	const S lower{ 0 };
	const S upper{ 1 };
	if (z <= math::epsilon<S>) {
		// First segment degenerates into a point
		out_s = 0;
		out_t = f / e; // out_s = 0 => out_t = (b*out_s + f) / e = f / e
		out_t = std::clamp(out_t, lower, upper);
	} else {
		const S c{ d1.Dot(r) };
		if (e <= math::epsilon<S>) {
			// Second segment degenerates into a point
			out_t = 0;
			out_s = std::clamp(-c / z, lower, upper); // out_t = 0 => out_s = (b*out_t - c) / z = -c / z
		} else {
			// The general nondegenerate case starts here
			const S b{ d1.Dot(d2) };
			const S denom{ z * e - b * b }; // Always nonnegative
			// If segments not parallel, compute closest point on L1 to L2 and
			// clamp to segment S1. Else pick arbitrary out_s (here 0)
			if (!math::Compare(denom, 0))
				out_s = std::clamp((b * f - c * e) / denom, lower, upper);
			else
				out_s = 0;

			const S tnom{ b * out_s + f };
			if (tnom < 0) {
				out_t = 0;
				out_s = std::clamp(-c / z, lower, upper);
			} else if (tnom > e) {
				out_t = 1;
				out_s = std::clamp((b - c) / z, lower, upper);
			} else {
				out_t = tnom / e;
			}
			/*
			// Compute point on L2 closest to S1(out_s) using
			// out_t = Dot((P1 + D1*out_s) - P2,D2) / Dot(D2,D2) = (b*out_s + f) / e
			out_t = (b * out_s + f) / e;
			// If out_t in [lower, upper] done. Else clamp out_t, recompute out_s for the new value
			// of out_t using out_s = Dot((P2 + D2*out_t) - P1,D1) / Dot(D1,D1)= (out_t*b - c) / z
			// and clamp out_s to [lower, upper]
			if (out_t < 0) {
				out_t = 0;
				out_s = Clamp(-c / z, lower, upper);
			} else if (out_t > 1) {
				out_t = 1;
				out_s = Clamp((b - c) / z, lower, upper);
			}
			*/
		}
	}
	out_c1 = a.origin + d1 * out_s;
	out_c2 = b.origin + d2 * out_t;
	const math::Vector2<S> subtraction{ out_c1 - out_c2 };
	return subtraction.Dot(subtraction);
}

} // namespace math

namespace overlap {

// Check if a capsule and an AABB overlap.
// Capsule origin and destination are taken from the edge circle centers.
template <typename T, typename S = double,
	tt::floating_point<S> = true>
static bool CapsuleAABB(const Capsule<T>& a,
						const AABB<T>& b) {
	static_assert(!tt::is_narrowing_v<T, S>);
	using Edge = std::pair<math::Vector2<T>, math::Vector2<T>>;
	math::Vector2<T> top_right{ b.position.x + b.size.x, b.position.y };
	math::Vector2<T> bottom_right{ b.position + b.size };
	math::Vector2<T> bottom_left{ b.position.x, b.position.y + b.size.y };
	std::array<Edge, 4> edges;
	edges.at(0) = { b.position, top_right };
	edges.at(1) = { top_right, bottom_right };
	edges.at(2) = { bottom_right, bottom_left };
	edges.at(3) = { bottom_left, b.position };
	S minimum_distance{ std::numeric_limits<S>::infinity() };
	math::Vector2<S> min_capsule;
	//math::Vector2<T> minimum_edge_point;
	// Find shortest distance between capsule line and AABB by iterating over each edge of the AABB.
	for (auto& [origin, destination] : edges) {
		S s{};
		S t{};
		math::Vector2<S> c1;
		math::Vector2<S> c2;
		const S distance_squared{ math::ClosestPointLineLine<S>(a, { origin, destination },
																s, t, c1, c2) };
		if (distance_squared < minimum_distance) {
			minimum_distance = distance_squared;
			// Point on the capsule that was the closest.
			min_capsule = c1;
			//minimum_edge_point = c2;
		}
	}
	// Simply check if the closest point on the capsule (as a circle) overlaps with the AABB.
	return CircleAABB({ min_capsule, static_cast<S>(a.radius) }, static_cast<AABB<S>>(b));
}

// Source: http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
// Pages 114-115; pages 149-150 for ClosestPointLineLine function.
// Check if two capsules overlap.
// Capsule origins and destinations are taken from the edge circle centers.
template <typename S = double, typename T,
	tt::floating_point<S> = true>
static bool CapsuleCapsule(const Capsule<T>& a,
						   const Capsule<T>& b) {
	// Compute (squared) distance between the inner structures of the capsules.
	S s{};
	S t{};
	math::Vector2<S> c1;
	math::Vector2<S> c2;
	const S dist2{ math::ClosestPointLineLine<S>(a, b, s, t, c1, c2) };
	static_assert(!tt::is_narrowing_v<T, S>);
	// If (squared) distance smaller than (squared) sum of radii, they collide
	const S rad_sum{ static_cast<S>(a.radius) + static_cast<S>(b.radius) };
	const S rad_sum2{ rad_sum * rad_sum };
	return dist2 < rad_sum2 || math::Compare(dist2, rad_sum2);
}

} // namespace overlap

} // namespace ptgn