#pragma once

#include "math/Vector2.h"
#include "math/Math.h"
#include "utility/TypeTraits.h"
#include "collision/Types.h"

namespace ptgn {

namespace math {

// Source: file:///C:/Users/Martin/Desktop/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
// Page 130.
// Returns the squared distance between point and segment line_origin -> line_destination.
template <typename T, typename S = double,
	tt::floating_point<S> = true>
static S PointToLineSquareDistance(const Point<T>& p,
									  const Line<T>& a) {
	const math::Vector2<S> ab{ a.destination - a.origin };
	const math::Vector2<S> ac{ p.p - a.origin };
	const math::Vector2<S> bc{ p.p - a.destination };
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
	tt::floating_point<S> = true>
static void ClosestPointLine(const Point<T>& p,
						  const Line<T>& a,
						  S& out_t,
						  math::Vector2<S>& out_d) {
	const math::Vector2<S> ab{ a.destination - a.origin };
	// Project c onto ab, but deferring divide by Dot(ab, ab)
	out_t = (p.p - a.origin).Dot(ab);
	if (out_t < 0 || math::Compare(out_t, 0)) {
		// c projects outside the [a,b] interval, on the a side; clamp to a
		out_t = 0;
		out_d = a.origin;
	} else {
		S denom = ab.Dot(ab); // Always nonnegative since denom = ||ab||^2
		if (out_t > denom || math::Compare(out_t, denom)) {
			// c projects outside the [a,b] interval, on the b side; clamp to b
			out_t = 1;
			out_d = a.destination;
		} else {
			// c projects inside the [a,b] interval; must do deferred divide now
			out_t = out_t / denom;
			out_d = a.origin + out_t * ab;
		}
	}
}

template <typename T>
T SquareDistancePointAABB(const Point<T>& p, const AABB<T>& a) {
	T dist2{ 0 };
	const Vector2<T> max{ a.Max() };
	for (std::size_t i{ 0 }; i < 2; ++i) {
		const T v{ p.p[i] };
		if (v < a.position[i]) dist2 += (a.position[i] - v) * (a.position[i] - v);
		if (v > max[i]) dist2 += (v - max[i]) * (v - max[i]);
	}
	return dist2;
}

} // namespace math

namespace overlap {

// Source: http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
// Page 165-166.
// Check if a circle and an AABB overlap.
// AABB position is taken from top left.
// AABB size is the full extent from top left to bottom right.
// Circle position is taken from its center.
template <typename T>
inline bool CircleAABB(const Circle<T>& a,
					   const AABB<T>& b) {
	const T dist2{ math::SquareDistancePointAABB(Point{ a.center }, b) };
	const T rad2{ a.RadiusSquared() };
	return dist2 < rad2 || math::Compare(dist2, rad2);
}

// Source: http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
// Page 114; page 130 for PointToLineSquareDistance function.
// Check if a circle and a capsule overlap.
// Capsule origin and destination are taken from the edge circle centers.
template <typename T, typename S = double,
	tt::floating_point<S> = true>
static bool CircleCapsule(const Circle<T>& a,
						  const Capsule<T>& b) {
	static_assert(!tt::is_narrowing_v<T, S>);
	// Compute (squared) distance between sphere center and capsule line segment.
	const S dist2{ math::PointToLineSquareDistance<S>(Point{ a.center },
													  Line{ b.origin, b.destination }) };
	// If (squared) distance smaller than (squared) sum of radii, they collide.
	const T rad_sum{ a.radius + b.radius };
	const S rad_sum2{ static_cast<S>(rad_sum) * static_cast<S>(rad_sum) };
	return dist2 < rad_sum2 || math::Compare(dist2, rad_sum2);
}

// Source: http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
// Page 88.
// Check if two circles overlap.
// Circle positions are taken from their centers.
template <typename T>
inline bool CircleCircle(const Circle<T>& a,
						 const Circle<T>& b) {
	const math::Vector2<T> dist{ a.center - b.center };
	const T dist2{ dist.Dot(dist) };
	const T rad_sum{ a.radius + b.radius };
	const T rad_sum2{ rad_sum * rad_sum };
	return dist2 < rad_sum2 || math::Compare(dist2, rad_sum2);
}

} // namespace overlap

} // namespace ptgn