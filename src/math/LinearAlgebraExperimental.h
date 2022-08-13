#pragma once

#include <algorithm> // std::clamp

#include "math/Vector2.h"
#include "utility/TypeTraits.h"
#include "physics/Types.h"

namespace ptgn {

namespace math {

// Computes closest points out_c1 and out_c2 of
// S1(out_s) = a.origin + out_s * (a.destination - a.origin)
// S2(out_t) = b.origin + out_t * (b.destination - b.origin)
template <typename T = float,
	tt::floating_point<T> = true>
void ClosestPointsSegmentSegment(const Segment<T>& a,
								 const Segment<T>& b,
								 Point<T>& out_c1,
								 Point<T>& out_c2,
								 T& out_s,
								 T& out_t) {
	const auto d1{ a.Direction() };
	const auto d2{ b.Direction() };
	const auto r{ a.origin - b.origin };
	const T mag_a2{ d1.MagnitudeSquared() }; // Squared length of segment S1, always nonnegative
	const T mag_b2{ d2.MagnitudeSquared() }; // Squared length of segment S2, always nonnegative
	// Check if either or both segments degenerate into points
	bool a_point{ math::Compare(mag_a2, 0) };
	bool b_point{ math::Compare(mag_b2, 0) };
	if (a_point && b_point) {
		// Both segments degenerate into points.
		out_s = out_t = 0;
		out_c1 = a.origin;
		out_c2 = b.origin;
		return;
	} else if (a_point) {
		// First segment degenerates into a point.
		const T bdr{ d2.Dot(r) };
		out_s = 0;
		out_t = bdr / mag_b2; // out_s = 0 => out_t = (b * out_s + bdr) / mag_b2 = bdr / mag_b2
		out_t = std::clamp(out_t, 0.0f, 1.0f);
	} else if (b_point) {
		// Second segment degenerates into a point.
		const T adr{ d1.Dot(r) };
		out_t = 0;
		out_s = std::clamp(-adr / mag_a2, 0.0f, 1.0f); // out_t = 0 => out_s = (b * out_t - adr) / mag_a2 = -adr / mag_a2
	} else {
		const T adr{ d1.Dot(r) };
		const T bdr{ d2.Dot(r) };
		// The general non-degenerate case starts here.
		const T adb{ d1.Dot(d2) };
		const T denom{ mag_a2 * mag_b2 - adb * adb }; // Always nonnegative
		// If segments not parallel, compute closest point on L1 to L2 and
		// clamp to segment S1. Else pick arbitrary s (here 0)
		if (math::Compare(denom, 0))
			out_s = 0;
		else 
			out_s = std::clamp((adb * bdr - adr * mag_b2) / denom, 0.0f, 1.0f);
		const T tnom{ adb * out_s + bdr };
		if (tnom < 0) {
			out_t = 0;
			out_s = std::clamp(-adr / mag_a2, 0.0f, 1.0f);
		} else if (tnom > mag_b2) {
			out_t = 1;
			out_s = std::clamp((adb - adr) / mag_a2, 0.0f, 1.0f);
		} else {
			out_t = tnom / mag_b2;
		}
	}
	out_c1 = a.origin + d1 * out_s;
	out_c2 = b.origin + d2 * out_t;
}

// Given an infinite line line_origin->line_destination and point, computes closest point out_d on ab.
// Also returns out_t for the parametric position of out_d, out_d(t)= a + out_t * (b - a)
template <typename T = float,
	tt::floating_point<T> = true>
inline void ClosestPointLine(const Point<T>& a,
							 const Line<T>& b,
							 T& out_t,
							 Point<T>& out_d) {
	auto dir{ b.Direction() };
	// Project c onto ab, but deferring divide by Dot(ab, ab)
	out_t = (a - b.origin).Dot(dir) / dir.Dot(dir);
	out_d = b.origin + out_t * dir;
}

/*

// Source: http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
// Pages 149-150.
// Computes closest points C1 and C2 of S1(s)=P1+s*(Q1-P1) and
// S2(t)=P2+t*(Q2-P2), returning s and t.
// Function return is squared distance between between S1(s) and S2(t)
template <typename S = float, typename T,
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
}
}
out_c1 = a.origin + d1 * out_s;
out_c2 = b.origin + d2 * out_t;
const math::Vector2<S> subtraction{ out_c1 - out_c2 };
return subtraction.Dot(subtraction);
}


// Given an infinite line line_origin->line_destination and point, computes closest point out_d on ab.
// Also returns out_t for the parametric position of out_d, out_d(t)= a + out_t * (b - a)
template <typename S = float, typename T,
	tt::floating_point<S> = true>
	inline void ClosestPointInfiniteLine(const Point<T>& a,
										 const Line<T>& b,
										 S& out_t,
										 math::Vector2<S>& out_d) {
	math::Vector2<S> dir{ b.Direction() };
	// Project c onto ab, but deferring divide by Dot(ab, ab)
	out_t = (a - b.origin).Dot(dir) / dir.Dot(dir);
	out_d = b.origin + out_t * dir;
}


template <typename T = float,
	tt::floating_point<T> = true>
	Collision<T> CircleCapsule(const Circle<T>& a,
							   const Capsule<T>& b) {
	Collision<T> c;
	const auto ab{ b.Direction() };
	math::Vector2<T> p;
	// Project c onto ab, but deferring divide by Dot(ab, ab)
	const T t{ (a.center - b.origin).Dot(ab) };
	const T denom{ ab.MagnitudeSquared() }; // Always nonnegative since denom = ||ab||^2
	if (t > 0) {
		if (t < denom) {
			// c projects inside the [a,b] interval; must do deferred divide now
			p = b.origin + t / denom * ab;
		} else {
			// c projects outside the [a,b] interval, on the b side; clamp to b
			p = b.destination;
		}
	} else {
		// c projects outside the [a,b] interval, on the a side; clamp to a
		p = b.origin;
	}
	const T rad{ a.radius + b.radius };
	const auto dir{ p - a.center };
	const T dist2{ dir.MagnitudeSquared() };
	if (dist2 < rad * rad) {
		const T dist{ std::sqrtf(dist2) };
		if (math::Compare(dist, 0))
			c.normal = -ab.Tangent() / std::sqrtf(denom);
		else
			c.normal = dir / dist;
		c.count = 1;
		c.depth[0] = rad - dist;
		c.point[0] = p - c.normal * b.radius;
	}
	return c;
}

template <typename S = float, typename T,
	tt::floating_point<S> = true>
	static Collision<S> CapsuleCapsule(const Capsule<T>& a,
									   const Capsule<T>& b) {
	// TODO: Make this function much more streamlined.
	static_assert(!tt::is_narrowing_v<T, S>);
	Collision<S> collision;
	// Compute (squared) distance between the inner structures of the capsules.
	S s{};
	S t{};
	math::Vector2<S> c1;
	math::Vector2<S> c2;
	const S dist2{ math::ClosestPointLineLine<S>(a, b, s, t, c1, c2) };
	// If (squared) distance smaller than (squared) sum of radii, they collide
	const S rad_sum{ static_cast<S>(a.radius) + static_cast<S>(b.radius) };
	const S rad_sum2{ rad_sum * rad_sum };
	if (!(dist2 < rad_sum2 ||
		  math::Compare(dist2, rad_sum2))) {
		return collision;
	}
	collision.count = 1;
	if (math::Compare(dist2, 0)) {
		// Capsules lines intersect, different kind of routine needed.
		std::array<math::Vector2<S>, 4> points;
		points[0] = a.origin;
		points[1] = a.destination;
		points[2] = b.origin;
		points[3] = b.destination;

		// Find shortest distance (and index) to 4 capsule end points (2 per capsule).
		S min_dist2{ std::numeric_limits<S>::infinity() };
		std::size_t min_index{ 0 };
		std::size_t max_index{ 0 };
		for (std::size_t i{ 0 }; i < points.size(); ++i) {
			const S d{ DistanceSquared(points[i], c1) };
			if (d < min_dist2) {
				min_index = i;
				min_dist2 = d;
			}
		}
		Line<S> line{ a };
		Line<S> other{ b };
		S sign{ -1 };
		// Determine which is the which is the collision normal axis
		// and set the non collision normal axis as the other one.
		if (min_index == 0) {
			max_index = 1;
		} else if (min_index == 1) {
			max_index = 0;
		} else if (min_index == 2) {
			Swap(line.origin, other.origin);
			Swap(line.destination, other.destination);
			sign = 1;
			max_index = 3;
		} else if (min_index == 3) {
			Swap(line.origin, other.origin);
			Swap(line.destination, other.destination);
			sign = 1;
			max_index = 2;
		}
		math::Vector2<S> dir{ line.Direction() };
		math::Vector2<S> o_dir{ other.Direction() };
		// TODO: Perhaps this check could be moved to the very beginning as it does not rely on projections.
		bool zero_dir{ dir.IsZero() };
		bool o_zero_dir{ o_dir.IsZero() };
		if (zero_dir || o_zero_dir) {
			// At least one of the capsules is a circle.
			if (zero_dir && o_zero_dir) {
				// Both capsules are circles.
				// Circle vs circle collision where both circle centers overlap.
				return CircleCircle(Circle{ c1, static_cast<S>(a.radius) }, Circle{ c2, static_cast<S>(b.radius) });
			} else {
				if (zero_dir) {
					// Only one of the capsules is a circle.
					// Capsule vs circle collision where circle center intersects capsule centerline.
					collision.normal = o_dir.Tangent().Normalize();
					collision.depth[0] = rad_sum;
					return collision;
				} else if (o_zero_dir) {
					// Only one of the capsules is a circle.
					// Capsule vs circle collision where circle center intersects capsule centerline.
					collision.normal = dir.Tangent().Normalize();
					collision.depth[0] = rad_sum;
					return collision;
				}
			}
		} else {
			// Capsule vs capsule.
			S frac{}; // frac is an unused variable.
			math::Vector2<S> point;
			// TODO: Fix this awful branching.
			// TODO: Clean this up, I'm sure some of these cases can be combined.
			math::ClosestPointInfiniteLine(points[min_index], other, frac, point);
			const math::Vector2<S> vector_to_min{ points[min_index] - point };
			if (vector_to_min.IsZero()) {
				// Capsule centerlines touch in at least one location.
				math::ClosestPointInfiniteLine(points[max_index], other, frac, point);
				const math::Vector2<S> vector_to_max{ -(points[max_index] - point).Normalize() };
				if (vector_to_max.IsZero()) {
					// Capsules are collinear.
					const S penetration{ Distance(points[min_index], point) + rad_sum };
					if (penetration > rad_sum) {
						// Push capsules apart in perpendicular direction.
						collision.normal = -dir.Tangent().Normalize();
						collision.depth[0] = rad_sum;
						return collision;
					} else {
						// Push capsules apart in parallel direction.
						collision.normal = sign * -dir.Normalize();
						collision.depth[0] = penetration;
						return collision;
					}
				} else {
					// Capsule origin or destination lies on the other capsule's centerline.
					collision.normal = sign * vector_to_max;
					collision.depth[0] = rad_sum;
					return collision;
				}
			} else {
				// Capsule centerlines intersect each other.
				collision.normal = sign * vector_to_min.Normalize();
				collision.depth[0] = (Distance(points[min_index], point) + rad_sum);
				return collision;
			}
		}
	} else {
		// Capsule centerlines do not intersect each other.
		return CircleCircle(Circle{ c1, static_cast<S>(a.radius) }, Circle{ c2, static_cast<S>(b.radius) });
	}
	return collision;
}
*/

} // namespace math

} // namespace ptgn