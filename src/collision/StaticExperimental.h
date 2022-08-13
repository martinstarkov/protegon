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
    bool Occured() const {
        return count;
    }
	T penetration{ 0 };
    int count{ 0 };
	T depth[2]{};
    math::Vector2<T> point[2];
    math::Vector2<T> normal;
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
		c.count = 1;
		c.depth[0] = rad - dist;
		c.point[0] = b.center - c.normal * b.radius;
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

	c.count = 1;
	c.point[0] = mid_a;
	if (dx < dy) {
		c.depth[0] = dx;
		if (d.x < 0) {
			c.normal.x = -1;
			c.point[0].x -= eA.x;
		} else {
			c.normal.x = 1;
			c.point[0].x += eA.x;
		}
	} else {
		c.depth[0] = dy;
		if (d.y < 0) {
			c.normal.y = -1;
			c.point[0].y -= eA.y;
		} else {
			c.normal.y = 1;
			c.point[0].y += eA.y;
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
		c.count = 1;
		if (!math::Compare(dist2, 0)) {
			// shallow (center of circle not inside of AABB).
			const T dist{ std::sqrtf(dist2) };
			c.normal = dir / dist;
			c.depth[0] = a.radius - dist;
			c.point[0] = a.center + c.normal * dist;
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

			c.depth[0] = a.radius + depth;
			c.point[0] = a.center - c.normal * depth;
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
		c.count = 1;
		c.depth[0] = rad - dist;
		c.point[0] = p - c.normal * b.radius;
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
			assert(c1 == c2);
			const auto a_dir{ a.Direction() };
			const auto b_dir{ b.Direction() };
			bool a_circle{ a_dir.IsZero() };
			bool b_circle{ b_dir.IsZero() };
			if (a_circle && b_circle) {
				c.normal.y = -1;
				c.depth[0] = rad;
				//return CircleCircle(Circle{ c1, a.radius }, Circle{ c2, b.radius });
			} else if (a_circle) {
				c.normal = -b_dir.Tangent().Normalize();
				c.depth[0] = rad;
			} else if (b_circle) {
				c.normal = -a_dir.Tangent().Normalize();
				c.depth[0] = rad;
			} else {
				// Capsules lines intersect, different kind of routine needed.
				std::array<math::Vector2<T>, 4> points{ a.origin, a.destination, b.origin, b.destination };
				// Find shortest distance (and index) to 4 capsule end points (2 per capsule).
				T min_dist2{ std::numeric_limits<T>::infinity() };
				std::size_t min_index{ 0 };
				std::size_t max_index{ 0 };
				for (std::size_t i{ 0 }; i < points.size(); ++i) {
					const T d{ DistanceSquared(points[i], c1) };
					if (d < min_dist2) {
						min_index = i;
						min_dist2 = d;
					}
				}
				Line<T> line{ a };
				Line<T> other{ b };
				T sign{ -1 };
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
				math::Vector2<T> i_dir{ line.Direction() };
				math::Vector2<T> o_dir{ other.Direction() };
				// Capsule vs capsule.
				T frac{}; // frac is an unused variable.
				Point<T> point;
				// TODO: Fix this awful branching.
				// TODO: Clean this up, I'm sure some of these cases can be combined.
				math::ClosestPointLine(points[min_index], other, frac, point);
				const auto vector_to_min{ points[min_index] - point };
				if (vector_to_min.IsZero()) {
					// Capsule centerlines touch in at least one location.
					math::ClosestPointLine(points[max_index], other, frac, point);
					const auto vector_to_max{ -(points[max_index] - point).Normalize() };
					if (vector_to_max.IsZero()) {
						// Capsules are collinear.
						const T penetration{ Distance(points[min_index], point) + rad };
						if (penetration > rad) {
							// Push capsules apart in perpendicular direction.
							c.normal = -i_dir.Tangent().Normalize();
							c.depth[0] = rad;
						} else {
							// Push capsules apart in parallel direction.
							c.normal = -sign * -i_dir.Normalize();
							c.depth[0] = penetration;
						}
					} else {
						// Capsule origin or destination lies on the other capsule's centerline.
						c.normal = -sign * vector_to_max;
						c.depth[0] = rad;
					}
				} else {
					// Capsule centerlines intersect each other.
					c.normal = -sign * vector_to_min.Normalize();
					c.depth[0] = (Distance(points[min_index], point) + rad);
				}
				/*
				// Two capsules with intersecting centerlines.
				// Fractional distances to end points.
				const std::array<T, 4> f{ s, 1 - s, t, 1 - t };
				const std::array<math::Vector2<T>, 4> ep{ a.origin, a.destination, b.origin, b.destination };
				// Determine which end of both capsules is closest to intersection point.
				const auto min_i{ std::distance(std::begin(f), std::min_element(std::begin(f), std::end(f))) };
				const auto half{ min_i / 2 };
				// This code replaces the 4 if-statements below but is less readable.
				const auto max_i{ half < 1 ? (min_i + 1) % 2 : (min_i - 1) % 2 + 2 };
				Line<T> line{ b };
				math::Vector2<T> proj{ b_dir };
				if (half < 1) {
					line = a;
					proj = a_dir;
				}

				const auto n{ proj / proj.MagnitudeSquared() };
				// Project point onto infinite line.
				auto point_to_line = [&](auto& point) {
					return line.origin + (point - line.origin).Dot(proj) * n;
				};
				auto p{ point_to_line(ep[min_i]) };
				const auto to_min{ ep[min_i] - p };

				if (to_min.IsZero()) {
					// At least one capsule centerline end point is on the other capsule centerline.
					p = point_to_line(ep[max_i]);
					const auto to_max{ ep[max_i] - p };
					if (to_max.IsZero()) {
						// Capsule centerlines are collinear.
						const T pen2{ DistanceSquared(ep[min_i], p) };
						if (pen2 > 0) {
							// Push capsules apart in perpendicular direction.
							c.normal = a_dir.Tangent().Normalize();
						} else {
							// Push capsules apart in parallel direction.
							c.normal = a_dir.Normalize();
						}
						c.depth[0] = rad;
					} else {
						// Capsule origin or destination lies on the other capsule's centerline.
						c.normal = to_max.Normalize();
						c.depth[0] = rad;
					}
				} else {
					// Capsule centerlines intersect each other.
					c.normal = to_min.Normalize();
					c.depth[0] = rad + Distance(ep[min_i], p);
				}
				*/
			}
		} else {
			T dist{ std::sqrtf(dist2) };
			assert(!math::Compare(dist, 0));
			c.normal = dir / dist;
			c.depth[0] = rad - dist;
			c.point[0] = c2 - c.normal * b.radius;
		}
		c.count = 1;
	}
	return c;
}


} // namespace intersect

} // namespace ptgn