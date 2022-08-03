#pragma once

#include "math/Vector2.h"
#include "math/Math.h"
#include "collision/fixed/FixedCollision.h"
#include "collision/overlap/OverlapCapsuleCapsule.h"
#include "collision/overlap/OverlapCircleCapsule.h"
#include "collision/fixed/FixedCircleCircle.h"

// TODO: TEMP
#include "utility/Log.h"
#include "interface/Draw.h"

// Source: https://steamcdn-a.akamaihd.net/apps/valve/2015/DirkGregorius_Contacts.pdf

namespace ptgn {

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
	const math::Vector2<S> direction{ capsule_destination - capsule_origin };
	const math::Vector2<S> other_direction{ other_capsule_destination - other_capsule_origin };
	const S slope{ direction.y / direction.x };
	const S other_slope{ other_direction.y / other_direction.x };
	if (math::Compare(slope, other_slope)) {
		//Capsules lines are parallel, normal is tangent to either.
		collision.normal = (-direction).Tangent().Unit();
		collision.penetration = collision.normal * (capsule_radius + other_capsule_radius - math::Sqrt(distance_squared));
	} else if (math::Compare(distance_squared, static_cast<S>(0))) {
		// Capsules lines intersect, different kind of routine needed.
		// TODO: Fix line intersections.
		math::Vector2<S> minimum_distance_point{ capsule_destination };
		math::Vector2<S> maximum_distance_point{ capsule_origin };
		S max_distance_squared{ DistanceSquared(capsule_origin, c1) };
		S min_distance_squared{ DistanceSquared(capsule_destination, c1) };
		if (min_distance_squared > max_distance_squared) {
			std::swap(min_distance_squared, max_distance_squared);
			Swap(minimum_distance_point, maximum_distance_point);
		}
		const math::Vector2<S> vector_to_max{ maximum_distance_point - c1 };
		const S dot_product{ vector_to_max.DotProduct(other_direction.Unit()) };
		const S p{ math::Sqrt(max_distance_squared / (max_distance_squared - dot_product * dot_product) * other_capsule_radius * other_capsule_radius) };
		const S pen1{ (minimum_distance_point - c1).Magnitude() + p };

		collision.normal = vector_to_max.Unit();

		const math::Vector2<S> new_point{ minimum_distance_point + collision.normal * pen1 };
		const math::Vector2<S> vector_to_end{ maximum_distance_point - new_point };
		const S distance_to_end_squared{ vector_to_end.MagnitudeSquared() };
		const S dot_product_to_end{ vector_to_end.DotProduct(other_direction.Unit()) };
		const S p2{ math::Sqrt(distance_to_end_squared / (distance_to_end_squared - dot_product_to_end * dot_product_to_end) * capsule_radius * capsule_radius) };
		const S pen2{ p2 };

		collision.penetration = collision.normal * (pen1 + pen2);
		
	} else {
		return CirclevsCircle(c1, static_cast<S>(capsule_radius), c2, static_cast<S>(other_capsule_radius));
	}
	return collision;
}

} // namespace fixed

} // namespace collision

} // namespace ptgn