#pragma once

#include <array>
#include <limits>

#include "math/LinearAlgebraExperimental.h"
#include "math/Math.h"
#include "math/Vector2.h"
#include "physics/Types.h"
#include "utility/TypeTraits.h"

namespace ptgn {

namespace intersect {

template <typename T,
    tt::floating_point<T> = true>
struct Collision {
    Collision() = default;
    ~Collision() = default;
	// TODO: Consider making this variable private.
    bool occured{ false };
	T depth{ 0 };
    math::Vector2<T> normal;
    //math::Vector2<T> point[2];
};

template <typename T = float,
	tt::floating_point<T> = true>
Collision<T> CircleCircle(const Circle<T>& a,
						  const Circle<T>& b) {
	Collision<T> c;
	const auto dir{ b.center - a.center };
	const T dist2{ dir.MagnitudeSquared() };
	const T rad{ a.radius + b.radius };
	if (dist2 < rad * rad) {
		const T dist{ std::sqrtf(dist2) };
		if (math::Compare(dist, 0))
			c.normal.y = -1;
		else
			c.normal = dir / dist;
		c.occured = true;
		c.depth = rad - dist;
		//c.point[0] = b.center - c.normal * b.radius;
	}
	return c;
}

template <typename T = float,
	tt::floating_point<T> = true>
Collision<T> AABBAABB(const AABB<T>& a,
				      const AABB<T>& b) {
	Collision<T> c;
	const auto eA{ a.Half() };
	const auto eB{ b.Half() };
	const auto mid_a{ a.position + eA };
	const auto mid_b{ b.position + eB };
	const auto d{ mid_b - mid_a };

	const T dx{ eA.x + eB.x - math::FastAbs(d.x) };
	if (dx < 0) return c;
	const T dy{ eA.y + eB.y - math::FastAbs(d.y) };
	if (dy < 0) return c;

	c.occured = true;
	//c.point[0] = mid_a;
	if (dx < dy) {
		c.depth = dx;
		if (d.x < 0) {
			c.normal.x = -1;
			//c.point[0].x -= eA.x;
		} else {
			c.normal.x = 1;
			//c.point[0].x += eA.x;
		}
	} else {
		c.depth = dy;
		if (d.y < 0) {
			c.normal.y = -1;
			//c.point[0].y -= eA.y;
		} else {
			c.normal.y = 1;
			//c.point[0].y += eA.y;
		}
	}
	return c;
}

template <typename T = float,
	tt::floating_point<T> = true>
Collision<T> CircleAABB(const Circle<T>& a, 
						const AABB<T>& b) {
	Collision<T> c;
	const auto clamped{ math::Clamp(a.center, b.Min(), b.Max()) };
	const auto dir{ clamped - a.center };
	const T dist2{ dir.MagnitudeSquared() };
	const T rad2{ a.RadiusSquared() };
	if (dist2 < rad2) {
		c.occured = true;
		if (!math::Compare(dist2, 0)) {
			// shallow (center of circle not inside of AABB).
			const T dist{ std::sqrtf(dist2) };
			c.normal = dir / dist;
			c.depth = a.radius - dist;
			//c.point[0] = a.center + c.normal * dist;
		} else {
			// deep (center of circle inside of AABB)
			// clamp circle's center to edge of AABB, then form the manifold.
			const auto e{ b.Half() };
			const auto d{ a.center - b.Center() };
			const auto abs_d{ math::FastAbs(d) };

			const T overlap_x{ e.x - abs_d.x };
			const T overlap_y{ e.y - abs_d.y };
			T depth{ 0 };

			if (overlap_x < overlap_y) {
				depth = overlap_x;
				c.normal.x = -1;
				if (d.x < 0)
					c.normal.x = 1;
			} else {
				depth = overlap_y;
				c.normal.y = -1;
				if (d.y < 0)
					c.normal.y = 1;
			}

			c.depth = a.radius + depth;
			//c.point[0] = a.center - c.normal * depth;
		}
	}
	return c;
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
		c.occured = true;
		c.depth = rad - dist;
		//c.point[0] = p - c.normal * b.radius;
	}
	return c;
}

template <typename T = float,
	tt::floating_point<T> = true>
Collision<T> CapsuleCapsule(const Capsule<T>& a,
							const Capsule<T>& b) {
	Collision<T> c;
	math::Vector2<T> c1;
	math::Vector2<T> c2;
	T s{ 0 };
	T t{ 0 };
	math::ClosestPointsSegmentSegment(a, b, c1, c2, s, t);
	const auto dir{ c2 - c1 };
	const T dist2{ dir.MagnitudeSquared() };
	const T rad{ a.radius + b.radius };
	if (dist2 < rad * rad) {
		if (math::Compare(dist2, 0)) {
			
			const T mag_a2{ a.Direction().MagnitudeSquared() }; // Squared length of segment S1, always nonnegative
			const T mag_b2{ b.Direction().MagnitudeSquared() }; // Squared length of segment S2, always nonnegative
			// Check if either or both segments degenerate into points
			bool a_point{ math::Compare(mag_a2, 0) };
			bool b_point{ math::Compare(mag_b2, 0) };
			if (a_point && b_point) {
				return CircleCircle({ a.origin, a.radius }, { b.origin, b.radius });
			} else if (a_point) {
				return CircleCapsule({ a.origin, a.radius }, b);
			} else if (b_point) {
				c = CircleCapsule({ b.origin, b.radius }, a);
				c.normal *= -1;
				return c;
			}

			// Capsules lines intersect, different kind of routine needed.
			const T mag_a{ std::sqrtf(mag_a2) };
			const T mag_b{ std::sqrtf(mag_b2) };
			const std::array<T, 4> f{ s * mag_a, (1 - s) * mag_a, t * mag_b, (1 - t) * mag_b };
			const std::array<math::Vector2<T>, 4> ep{ a.origin, a.destination, b.origin, b.destination };
			// Determine which end of both capsules is closest to intersection point.
			const auto min_i{ std::distance(std::begin(f), std::min_element(std::begin(f), std::end(f))) };
			const auto half{ min_i / 2 };
			const auto sign{ 1 - 2 * half };
			// This code replaces the 4 if-statements below but is less readable.
			const auto max_i{ half < 1 ? (min_i + 1) % 2 : (min_i - 1) % 2 + 2 };
			T min_dist2 = f[min_i];
			Line<T> line{ a.origin, a.destination };
			Line<T> other{ b.origin, b.destination };
			if (half > 0) {
				Swap(line.origin, other.origin);
				Swap(line.destination, other.destination);
			}
			// Capsule vs capsule.
			T frac{}; // frac is an unused variable.
			Point<T> point;
			// TODO: Fix this awful branching.
			// TODO: Clean this up, I'm sure some of these cases can be combined.
			math::ClosestPointLine(ep[min_i], other, frac, point);
			const auto to_min{ ep[min_i] - point };
			if (!to_min.IsZero()) {
				// Capsule centerlines intersect each other.
				c.normal = sign * to_min.Normalize();
				c.depth = (Distance(ep[min_i], point) + rad);
			} else {
				// Capsule centerlines touch in at least one location.
				math::ClosestPointLine(ep[max_i], other, frac, point);
				const auto to_max{ (point - ep[max_i]).Normalize() };
				if (!to_max.IsZero()) // Capsule origin or destination lies on the other capsule's centerline.
					c.normal = to_max;
				// Capsules are collinear.
				else if (DistanceSquared(ep[min_i], point) > 0) { // Push capsules apart in perpendicular direction.
					PrintLine(DistanceSquared(ep[min_i], point));
					c.normal = line.Direction().Tangent().Normalize();
				} else { // Push capsules apart in parallel direction.
					PrintLine(DistanceSquared(ep[min_i], point));
					c.normal = line.Direction().Normalize();
				}
				c.depth = rad;
			}
		} else {
			T dist{ std::sqrtf(dist2) };
			assert(!math::Compare(dist, 0));
			c.normal = dir / dist;
			c.depth = rad - dist;
			//c.point[0] = c2 - c.normal * b.radius;
		}
		c.occured = true;
	}
	return c;
}


} // namespace intersect

} // namespace ptgn