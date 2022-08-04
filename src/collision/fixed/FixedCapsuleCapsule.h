#pragma once

#include <array> // std::array

#include "math/Vector2.h"
#include "math/Math.h"
#include "collision/fixed/FixedCollision.h"
#include "collision/overlap/OverlapCapsuleCapsule.h"
#include "collision/overlap/OverlapCircleCapsule.h"
#include "collision/fixed/FixedCircleCircle.h"

// Source: https://steamcdn-a.akamaihd.net/apps/valve/2015/DirkGregorius_Contacts.pdf

namespace ptgn {

namespace math {

// Given an infinite line line_origin->line_destination and point, computes closest point out_d on ab.
// Also returns out_t for the parametric position of out_d, out_d(t)= a + out_t * (b - a)
template <typename S = double, typename T,
	std::enable_if_t<std::is_floating_point_v<S>, bool> = true>
void ClosestPointInfiniteLine(const math::Vector2<T>& point, const math::Vector2<T>& line_origin, const math::Vector2<T>& line_destination, S& out_t, math::Vector2<S>& out_d) {
	math::Vector2<S> ab{ line_destination - line_origin };
	// Project c onto ab, but deferring divide by Dot(ab, ab)
	out_t = (point - line_origin).DotProduct(ab) / ab.DotProduct(ab);
	out_d = line_origin + out_t * ab;
}

} // namespace math

namespace collision {

namespace fixed {

// Get the collision information of two overlapping capsules.
// Capsule origins and destinations are taken from the edge circle centers.
template <typename S = double, typename T,
	std::enable_if_t<std::is_floating_point_v<S>, bool> = true>
static Collision<S> CapsulevsCapsule(const math::Vector2<T>& capsule_origin,
									 const math::Vector2<T>& capsule_destination,
									 const T capsule_radius,
									 const math::Vector2<T>& other_capsule_origin,
									 const math::Vector2<T>& other_capsule_destination,
									 const T other_capsule_radius) {
	Collision<S> collision;
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
	if (!(distance_squared < combined_radius_squared ||
		math::Compare(distance_squared, combined_radius_squared))) {
		return collision;
	}
	collision.SetOccured();
	if (math::Compare(distance_squared, static_cast<S>(0))) {
		// Capsules lines intersect, different kind of routine needed.
		std::array<math::Vector2<S>, 4> points;
		points[0] = capsule_origin;
		points[1] = capsule_destination;
		points[2] = other_capsule_origin;
		points[3] = other_capsule_destination;

		// Find shortest distance (and index) to 4 capsule end points (2 per capsule).
		S min_distance_squared{ math::Infinity<S>() };
		std::size_t min_index{ 0 };
		std::size_t max_index{ 0 };
		for (std::size_t i{ 0 }; i < points.size(); ++i) {
			const S d{ DistanceSquared(points[i], c1) };
			if (d < min_distance_squared) {
				min_index = i;
				min_distance_squared = d;
			}
		}
		math::Vector2<S> origin{ capsule_origin };
		math::Vector2<S> destination{ capsule_destination };
		math::Vector2<S> other_origin{ other_capsule_origin };
		math::Vector2<S> other_destination{ other_capsule_destination };
		S sign{ -1 };
		// Determine which is the which is the collision normal axis
		// and set the non collision normal axis as the other one.
		if (min_index == 0) {
			max_index = 1;
		} else if (min_index == 1) {
			max_index = 0;
		} else if (min_index == 2) {
			Swap(origin, other_origin);
			Swap(destination, other_destination);
			sign = 1;
			max_index = 3;
		} else if (min_index == 3) {
			Swap(origin, other_origin);
			Swap(destination, other_destination);
			sign = 1;
			max_index = 2;
		}
		math::Vector2<S> dir{ destination - origin };
		math::Vector2<S> o_dir{ other_destination - other_origin };
		// TODO: Perhaps this check could be moved to the very beginning as it does not rely on projections.
		if (dir.IsZero()) {
			// At least one of the capsules is a circle.
			if (o_dir.IsZero()) {
				// Both capsules are circles.
				// Circle vs circle collision where both circle centers overlap.
				collision = CirclevsCircle(c1, static_cast<S>(capsule_radius), c2, static_cast<S>(other_capsule_radius));
			} else {
				// Only one of the capsules is a circle.
				// Capsule vs circle collision where circle center intersects capsule centerline.
				collision.normal = o_dir.Tangent().Unit();
				collision.penetration = collision.normal * combined_radius;
			}
		} else {
			// Capsule vs capsule.
			S frac; // frac is an unused variable.
			math::Vector2<S> point;
			// TODO: Clean this up, I'm sure some of these cases can be combined.
			math::ClosestPointInfiniteLine(points[min_index], other_origin, other_destination, frac, point);
			const math::Vector2<S> vector_to_min{ points[min_index] - point };
			if (vector_to_min.IsZero()) {
				// Capsule centerlines touch in at least one location.
				math::ClosestPointInfiniteLine(points[max_index], other_origin, other_destination, frac, point);
				const math::Vector2<S> vector_to_max{ -(points[max_index] - point).Unit() };
				if (vector_to_max.IsZero()) {
					// Capsules are collinear.
					const S penetration{ Distance(points[min_index], point) + combined_radius };
					if (penetration > combined_radius) {
						// Push capsules apart in perpendicular direction.
						collision.normal = -dir.Tangent().Unit();
						collision.penetration = collision.normal * combined_radius;
					} else {
						// Push capsules apart in parallel direction.
						collision.normal = sign * -dir.Unit();
						collision.penetration = collision.normal * penetration;
					}
				} else {
					// Capsule origin or destination lies on the other capsule's centerline.
					collision.normal = sign * vector_to_max;
					collision.penetration = collision.normal * combined_radius;
				}
			} else {
				// Capsule centerlines intersect each other.
				collision.normal = sign * vector_to_min.Unit();
				collision.penetration = collision.normal * (Distance(points[min_index], point) + combined_radius);
			}
		}
	} else {
		// Capsule centerlines do not intersect each other.
		collision = CirclevsCircle(c1, static_cast<S>(capsule_radius), c2, static_cast<S>(other_capsule_radius));
	}
	return collision;
}

} // namespace fixed

} // namespace collision

} // namespace ptgn