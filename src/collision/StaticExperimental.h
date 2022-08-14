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

struct Collision {
	bool Occured() const {
		return false;//depth >= 10.0f * std::numeric_limits<float>::epsilon();
	}
	float depth{ 0.0f };
    V2_float normal;
    //Vector2<T> point[2];
};

bool CircleCircle(const Circle<float>& A,
			      const Circle<float>& B,
				  Collision& c) {

	Vector2 d = B.c - A.c;
	float distSqr = Dot(d, d);
	float rA = A.r;
	float rB = B.r;
	float radius = rA + rB;
	if (distSqr > radius * radius) {
		return false;
	}

	//c.localPoint = circleA->m_p;
	//c.localNormal.SetZero();
	//c.pointCount = 1;
	//c.points[0].localPoint = circleB->m_p;
	//c.points[0].id.key = 0;
	c.normal = { 1.0f, 0.0f };
	if (DistanceSquared(A.c, B.c) > epsilon<float> * epsilon<float>) {
		c.normal = (B.c - A.c).Normalized();
	}

	Vector2 cA = A.c + A.r * c.normal;
	Vector2 cB = B.c - B.r * c.normal;
	//points[0] = 0.5f * (cA + cB);
	c.depth = Dot(cB - cA, c.normal);
	return true;
}

//Collision<float> CircleCircle(const Circle<float>& a,
//							  const Circle<float>& b) {
//	Collision<T> c;
//	const auto dir{ b.center - a.center };
//	const T dist2{ dir.MagnitudeSquared() };
//	const T rad{ a.radius + b.radius };
//	const T rad2{ rad * rad };
//	if (dist2 > rad2) return c;
//	T dist{ 0.0f };
//	if (dist2 > std::numeric_limits<float>::epsilon() * std::numeric_limits<float>::epsilon()) {
//		dist = std::sqrtf(dist2);
//		c.normal = dir / dist;
//	} else {
//		c.normal.y = -1.0f;
//	}
//	c.occured = true;
//	c.depth = rad - dist;
//	//c.point[0] = b.center - c.normal * b.radius;
//	return c;
//}

//template <typename T = float,
//	tt::floating_point<T> = true>
//Collision<T> AABBAABB(const AABB<T>& a,
//				      const AABB<T>& b) {
//	Collision<T> c;
//	const auto eA{ a.Half() };
//	const auto eB{ b.Half() };
//	const auto mid_a{ a.position + eA };
//	const auto mid_b{ b.position + eB };
//	const auto d{ mid_b - mid_a };
//
//	const T dx{ eA.x + eB.x - math::FastAbs(d.x) };
//	if (dx < 0) return c;
//	const T dy{ eA.y + eB.y - math::FastAbs(d.y) };
//	if (dy < 0) return c;
//
//	c.occured = true;
//	//c.point[0] = mid_a;
//	if (dx < dy) {
//		c.depth = dx;
//		if (d.x < 0) {
//			c.normal.x = -1;
//			//c.point[0].x -= eA.x;
//		} else {
//			c.normal.x = 1;
//			//c.point[0].x += eA.x;
//		}
//	} else {
//		c.depth = dy;
//		if (d.y < 0) {
//			c.normal.y = -1;
//			//c.point[0].y -= eA.y;
//		} else {
//			c.normal.y = 1;
//			//c.point[0].y += eA.y;
//		}
//	}
//	return c;
//}
//
//template <typename T = float,
//	tt::floating_point<T> = true>
//Collision<T> CircleAABB(const Circle<T>& a, 
//						const AABB<T>& b) {
//	Collision<T> c;
//	const auto clamped{ math::Clamp(a.center, b.Min(), b.Max()) };
//	const auto dir{ clamped - a.center };
//	const T dist2{ dir.MagnitudeSquared() };
//	const T rad2{ a.RadiusSquared() };
//	if (dist2 < rad2 || Compare(dist2, rad2)) {
//		c.occured = true;
//		if (!Compare(dist2, 0)) {
//			// shallow (center of circle not inside of AABB).
//			const T dist{ std::sqrtf(dist2) };
//			c.normal = dir / dist;
//			c.depth = a.radius - dist;
//			//c.point[0] = a.center + c.normal * dist;
//		} else {
//			// deep (center of circle inside of AABB)
//			// clamp circle's center to edge of AABB, then form the manifold.
//			const auto e{ b.Half() };
//			const auto d{ a.center - b.Center() };
//			const auto abs_d{ math::FastAbs(d) };
//
//			const T overlap_x{ e.x - abs_d.x };
//			const T overlap_y{ e.y - abs_d.y };
//			T depth{ 0 };
//
//			if (overlap_x < overlap_y) {
//				depth = overlap_x;
//				c.normal.x = -1;
//				if (d.x < 0)
//					c.normal.x = 1;
//			} else {
//				depth = overlap_y;
//				c.normal.y = -1;
//				if (d.y < 0)
//					c.normal.y = 1;
//			}
//
//			c.depth = a.radius + depth;
//			//c.point[0] = a.center - c.normal * depth;
//		}
//	}
//	return c;
//}
//
//template <typename T = float,
//	tt::floating_point<T> = true>
//Collision<T> CircleCapsule(const Circle<T>& a,
//						   const Capsule<T>& b) {
//	Collision<T> c;
//	const auto ab{ b.Direction() };
//	Vector2<T> p;
//	// Project c onto ab, but deferring divide by Dot(ab, ab)
//	const T t{ (a.center - b.origin).Dot(ab) };
//	const T denom{ ab.MagnitudeSquared() }; // Always nonnegative since denom = ||ab||^2
//	if (t > 0) {
//		if (t < denom) {
//			// c projects inside the [a,b] interval; must do deferred divide now
//			p = b.origin + t / denom * ab;
//		} else {
//			// c projects outside the [a,b] interval, on the b side; clamp to b
//			p = b.destination;
//		}
//	} else {
//		// c projects outside the [a,b] interval, on the a side; clamp to a
//		p = b.origin;
//	}
//	const auto dir{ p - a.center };
//	const T dist2{ dir.MagnitudeSquared() };
//	const T rad{ a.radius + b.radius };
//	const T rad2{ rad * rad };
//	if (dist2 < rad2 || Compare(dist2, rad2)) {
//		const T dist{ std::sqrtf(dist2) };
//		if (Compare(dist, 0))
//			c.normal = -ab.Tangent() / std::sqrtf(denom);
//		else 
//			c.normal = dir / dist;
//		c.occured = true;
//		c.depth = rad - dist;
//		//c.point[0] = p - c.normal * b.radius;
//	}
//	return c;
//}
//
//template <typename T = float,
//	tt::floating_point<T> = true>
//Collision<T> CapsuleCapsule(const Capsule<T>& a,
//							const Capsule<T>& b) {
//	Collision<T> c;
//	Vector2<T> c1;
//	Vector2<T> c2;
//	T s{ 0 };
//	T t{ 0 };
//	math::ClosestPointsSegmentSegment(a, b, c1, c2, s, t);
//	const auto dir{ c2 - c1 };
//	const T dist2{ dir.MagnitudeSquared() };
//	const T rad{ a.radius + b.radius };
//	const T rad2{ rad * rad };
//	if (dist2 - rad2 < 10.0f * std::numeric_limits<float>::epsilon()) {// || Compare(dist2, rad2)) {
//		if (Compare(dist2, 0)) {
//			
//			const T mag_a2{ a.Direction().MagnitudeSquared() }; // Squared length of segment S1, always nonnegative
//			const T mag_b2{ b.Direction().MagnitudeSquared() }; // Squared length of segment S2, always nonnegative
//			// Check if either or both segments degenerate into points
//			bool a_point{ Compare(mag_a2, 0) };
//			bool b_point{ Compare(mag_b2, 0) };
//			if (a_point && b_point) {
//				return CircleCircle({ a.origin, a.radius }, { b.origin, b.radius });
//			} else if (a_point) {
//				return CircleCapsule({ a.origin, a.radius }, b);
//			} else if (b_point) {
//				c = CircleCapsule({ b.origin, b.radius }, a);
//				c.normal *= -1;
//				return c;
//			}
//
//			// Capsules lines intersect, different kind of routine needed.
//			const T mag_a{ std::sqrtf(mag_a2) };
//			const T mag_b{ std::sqrtf(mag_b2) };
//			const std::array<T, 4> f{ s * mag_a, (1 - s) * mag_a, t * mag_b, (1 - t) * mag_b };
//			const std::array<Vector2<T>, 4> ep{ a.origin, a.destination, b.origin, b.destination };
//			// Determine which end of both capsules is closest to intersection point.
//			const auto min_i{ std::distance(std::begin(f), std::min_element(std::begin(f), std::end(f))) };
//			const auto half{ min_i / 2 };
//			const auto sign{ 1 - 2 * half };
//			// This code replaces the 4 if-statements below but is less readable.
//			const auto max_i{ half < 1 ? (min_i + 1) % 2 : (min_i - 1) % 2 + 2 };
//			T min_dist2 = f[min_i];
//			Line<T> line{ a.origin, a.destination };
//			Line<T> other{ b.origin, b.destination };
//			if (half > 0) {
//				Swap(line.origin, other.origin);
//				Swap(line.destination, other.destination);
//			}
//			// Capsule vs capsule.
//			T frac{}; // frac is an unused variable.
//			Point<T> point;
//			// TODO: Fix this awful branching.
//			// TODO: Clean this up, I'm sure some of these cases can be combined.
//			math::ClosestPointLine(ep[min_i], other, frac, point);
//			const auto to_min{ ep[min_i] - point };
//			if (!to_min.IsZero()) {
//				// Capsule centerlines intersect each other.
//				c.normal = sign * to_min.Normalize();
//				c.depth = (Distance(ep[min_i], point) + rad);
//			} else {
//				// Capsule centerlines touch in at least one location.
//				math::ClosestPointLine(ep[max_i], other, frac, point);
//				const auto to_max{ point - ep[max_i] };
//				PrintLine(to_max, ", ", Distance(ep[min_i], point));
//				if (!to_max.IsZero()) { // Capsule origin or destination lies on the other capsule's centerline.
//					c.normal = sign * to_max.Normalize();
//			    } else if (DistanceSquared(ep[min_i], point) > 0) { // Push capsules apart in perpendicular direction.
//					// Capsules are collinear.
//					c.normal = line.Direction().Tangent().Normalize();
//				} else { // Push capsules apart in parallel direction.
//					c.normal = line.Direction().Normalize();
//				}
//				c.depth = rad;
//			}
//		} else {
//			T dist{ std::sqrtf(dist2) };
//			assert(!Compare(dist, 0));
//			c.normal = dir / dist;
//			c.depth = rad - dist;
//			//c.point[0] = c2 - c.normal * b.radius;
//		}
//		c.occured = true;
//	}
//	return c;
//}


} // namespace intersect

} // namespace ptgn