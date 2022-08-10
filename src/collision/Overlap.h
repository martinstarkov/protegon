#pragma once

#include <array>  // std::array
#include <limits> // std::numeric_limits
#include <tuple>  // std::pair

#include "math/Vector2.h"
#include "math/Math.h"
#include "math/LinearAlgebra.h"
#include "utility/TypeTraits.h"
#include "physics/Types.h"

// TODO: Use segments instead of lines where appropriate.
// TODO: Check PointLine function correctness.

namespace ptgn {

namespace overlap {

// Source: http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
// Page 79.
template <typename T>
inline bool AABBAABB(const AABB<T>& a,
					 const AABB<T>& b) {
	if (a.position.x + a.size.x < b.position.x || a.position.x > b.position.x + b.size.x) return false;
	if (a.position.y + a.size.y < b.position.y || a.position.y > b.position.y + b.size.y) return false;
	return true;
}

// Source: http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
// Page 165-166.
template <typename T>
inline bool CircleAABB(const Circle<T>& a,
					   const AABB<T>& b) {
	const T dist2{ math::SquareDistancePointAABB(a.center, b) };
	const T rad2{ a.RadiusSquared() };
	return dist2 < rad2 || math::Compare(dist2, rad2);
}

// Source: http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
// Page 114.
template <typename T, typename S = float,
	tt::floating_point<S> = true>
static bool CircleCapsule(const Circle<T>& a,
						  const Capsule<T>& b) {
	static_assert(!tt::is_narrowing_v<T, S>);
	// Compute (squared) distance between sphere center and capsule line segment.
	const S dist2{ math::PointToLineSquareDistance<S>(a.center,
													  Line{ b.origin, b.destination }) };
	// If (squared) distance smaller than (squared) sum of radii, they collide.
	const T rad_sum{ a.radius + b.radius };
	const S rad_sum2{ static_cast<S>(rad_sum) * static_cast<S>(rad_sum) };
	return dist2 < rad_sum2 || math::Compare(dist2, rad_sum2);
}

// Source: http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
// Page 88.
template <typename T>
inline bool CircleCircle(const Circle<T>& a,
						 const Circle<T>& b) {
	const math::Vector2<T> dist{ a.center - b.center };
	const T dist2{ dist.Dot(dist) };
	const T rad_sum{ a.radius + b.radius };
	const T rad_sum2{ rad_sum * rad_sum };
	return dist2 < rad_sum2 || math::Compare(dist2, rad_sum2);
}

// Source: http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
// Pages 114-115.
template <typename S = float, typename T,
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

template <typename T, typename S = float,
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
	S min_dist2{ std::numeric_limits<S>::infinity() };
	math::Vector2<S> min_capsule;
	//math::Vector2<T> minimum_edge_point;
	// Find shortest distance between capsule line and AABB by iterating over each edge of the AABB.
	for (auto& [origin, destination] : edges) {
		S s{};
		S t{};
		math::Vector2<S> c1;
		math::Vector2<S> c2;
		const S dist2{ math::ClosestPointLineLine<S>(a, { origin, destination },
																s, t, c1, c2) };
		if (dist2 < min_dist2) {
			min_dist2 = dist2;
			// Point on the capsule that was the closest.
			min_capsule = c1;
			//minimum_edge_point = c2;
		}
	}
	// Simply check if the closest point on the capsule (as a circle) overlaps with the AABB.
	return CircleAABB({ min_capsule, static_cast<S>(a.radius) }, static_cast<AABB<S>>(b));
}

template <typename T, typename S = float,
	tt::floating_point<S> = true>
static bool LineAABB(const Line<T>& a,
					 const AABB<T>& b) {
	const math::Vector2<S> e{ b.size };
	const math::Vector2<S> d{ a.destination - a.origin };
	const math::Vector2<S> m{ a.origin + a.destination - 2 * b.position - b.size };
	// Try world coordinate axes as separating axes
	S adx{ math::FastAbs(d.x) };
	if (math::FastAbs(m.x) > e.x + adx) return false;
	S ady{ math::FastAbs(d.y) };
	if (math::FastAbs(m.y) > e.y + ady) return false;
	// Add in an epsilon term to counteract arithmetic errors when segment is
	// (near) parallel to a coordinate axis (see text for detail)
	adx += math::epsilon<S>;
	ady += math::epsilon<S>;
	// Try cross products of segment direction vector with coordinate axes
	if (math::FastAbs(m.x * d.y - m.y * d.x) > e.x * ady + e.y * adx) return false;
	// No separating axis found; segment must be overlapping AABB
	return true;

	// Alternative method:
	// Source: https://en.wikipedia.org/wiki/Cohen%E2%80%93Sutherland_algorithm
	//return math::CohenSutherlandLineClip(a.origin, a.destination, b.Min(), b.Max());
}

template <typename T, typename S = float,
	tt::floating_point<S> = true>
inline bool LineCapsule(const Line<T>& a,
					    const Capsule<T>& b) {
	return CapsuleCapsule({ a.origin, a.destination, 0 }, b);
}

// Source: https://www.jeffreythompson.org/collision-detection/line-circle.php
// Source: http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
// Page 179.
// Source: https://www.scratchapixel.com/lessons/3d-basic-rendering/minimal-ray-tracer-rendering-simple-shapes/ray-sphere-intersection
// Source (used): https://www.baeldung.com/cs/circle-line-segment-collision-detection
template <typename T, typename S = float,
	tt::floating_point<S> = true>
static bool LineCircle(const Line<T>& a,
					   const Circle<T>& b) {
	static_assert(!tt::is_narrowing_v<T, S>);
	// If the line is inside the circle entirely, exit early.
	if (PointCircle(a.origin, b) && PointCircle(a.destination, b)) return true;
	S min_dist2{ std::numeric_limits<S>::infinity() };
	const S rad2{ static_cast<S>(b.RadiusSquared()) };
	// O is the circle center, P is the line origin, Q is the line destination.
	const math::Vector2<S> OP{ a.origin - b.center };
	const math::Vector2<S> OQ{ a.destination - b.center };
	const math::Vector2<S> PQ{ a.destination - a.origin };
	const S OP_dist2{ OP.MagnitudeSquared() };
	const S OQ_dist2{ OQ.MagnitudeSquared() };
	const S max_dist2{ std::max(OP_dist2, OQ_dist2) };
	if (OP.Dot(-PQ) > 0 && OQ.Dot(PQ) > 0) {
		const S triangle_area{ math::FastAbs(math::ParallelogramArea<S>(b.center, a.origin, a.destination)) / S{ 2 } };
		min_dist2 = 4 * triangle_area * triangle_area / PQ.MagnitudeSquared();
	} else {
		min_dist2 = std::min(OP_dist2, OQ_dist2);
	}
	return (min_dist2 < rad2 || math::Compare(min_dist2, rad2)) &&
		(max_dist2 > rad2 || math::Compare(max_dist2, rad2));
}


// Source: https://www.geeksforgeeks.org/check-if-two-given-line-segments-intersect/
// With some modifications.
template <typename T>
static bool LineLine(const Line<T>& a,
					 const Line<T>& b) {
	// Sign of areas correspond to which side of ab points c and d are
	const T a1{ math::ParallelogramArea(a.origin, a.destination, b.destination) }; // Compute winding of abd (+ or -)
	const T a2{ math::ParallelogramArea(a.origin, a.destination, b.origin) }; // To intersect, must have sign opposite of a1
	// If c and d are on different sides of ab, areas have different signs
	bool polarity_diff{ false };
	bool collinear{ false };
	if constexpr (std::is_signed_v<T> && std::is_integral_v<T>) {
		// Second part for difference in polarity.
		polarity_diff = (a1 ^ a2) < 0;
		collinear = a1 == 0 || a2 == 0;
	} else {
		// Same as above but for floating points.
		polarity_diff = a1 * a2 < 0;
		collinear = math::Compare(a1, 0) || math::Compare(a2, 0);
	}
	if (!collinear && polarity_diff) {
		// Compute signs for a and b with respect to segment cd
		const T a3{ math::ParallelogramArea(b.origin, b.destination, a.origin) }; // Compute winding of cda (+ or -)
		// Since area is constant a1 - a2 = a3 - a4, or a4 = a3 + a2 - a1
		// const T a4 = math::ParallelogramArea(c, d, b); // Must have opposite sign of a3
		const T a4{ a3 + a2 - a1 };
		// Points a and b on different sides of cd if areas have different signs
		// Segments intersect if true.
		bool intersect{ false };
		// Check if a3 and a4 signs are different.
		if constexpr (std::is_signed_v<T> && std::is_integral_v<T>) {
			// If either is 0, the line is intersecting with the straight edge of the other line.
			// (i.e. corners with angles).
			intersect = (a3 ^ a4) < 0;
			collinear = a3 == 0 || a4 == 0;
		} else {
			// Same as above, hence the floating point comparison to 0.
			intersect = a3 * a4 < 0;
			collinear = math::Compare(a3, 0) || math::Compare(a4, 0);
		}
		if (intersect) return true;
	}
	return collinear &&
		  (math::PointLine(b.destination, a) ||
		   math::PointLine(b.origin, a)      ||
		   math::PointLine(a.origin, b)      ||
		   math::PointLine(a.destination, b));
}

template <typename T>
inline bool PointAABB(const Point<T>& a,
					  const AABB<T>& b) {
	return AABBAABB({ a, {} }, b);
}

template <typename T>
inline bool PointCapsule(const Point<T>& a,
						 const Capsule<T>& b) {
	return CircleCapsule({ a, 0 }, b);
}

template <typename T>
inline bool PointCircle(const Point<T>& a,
						const Circle<T>& b) {
	return CircleCircle({ a, 0 }, b);
}

/*
template <typename T>
inline bool PointPoint(const Point<T>& a,
					   const Point<T>& b) {
	return a == b;
}
*/

} // namespace overlap

} // namespace ptgn