#pragma once

#include "math/Vector2.h"
#include "collision/overlap/OverlapCircleCapsule.h"
#include "collision/fixed/FixedCollision.h"
#include "collision/fixed/FixedPointCircle.h"

namespace ptgn {

namespace collision {

namespace fixed {

// TODO: PERHAPS: Consider a faster alternative to using CapsulevsCapsule.

// Get the collision information of a point and a capsule.
// Capsule origin and destination are taken from the edge circle centers.
template <typename S = double, typename T,
	std::enable_if_t<std::is_floating_point_v<S>, bool> = true>
static Collision<S> PointvsCapsule(const math::Vector2<T>& point,
								   const math::Vector2<T>& capsule_origin,
								   const math::Vector2<T>& capsule_destination,
								   const T capsule_radius) {
	static_assert(!tt::is_narrowing_v<T, S>);
	Collision<S> collision;
	S t;
	math::Vector2<S> d;
	// Compute (squared) distance between sphere center and capsule line segment.
	math::ClosestPointLine<S>(point, capsule_origin, capsule_destination, t, d);
	const math::Vector2<S> vector{ d - point };
	const S distance_squared{ vector.MagnitudeSquared() };
	// If (squared) distance smaller than (squared) sum of radii, they collide.
	const S radius_squared{ static_cast<S>(capsule_radius) * static_cast<S>(capsule_radius) };
	if (!(distance_squared < radius_squared || math::Compare(distance_squared, radius_squared))) {
		return collision;
	}
	collision.SetOccured();
	if (math::Compare(distance_squared, 0)) {
		// Point is on the capsule's centerline.
		math::Vector2<S> dir{ capsule_destination - capsule_origin };
		if (dir.IsZero()) {
			// Point vs circle where point is at circle center.
			collision.normal = { 0, -1 };
			collision.penetration = collision.normal * capsule_radius;
		} else {
			// Point vs capsule where point is on capsule centerline.
			T min_distance1{ DistanceSquared(point, capsule_origin) };
			T min_distance2{ DistanceSquared(point, capsule_destination) };
			min_distance1 = math::Min(min_distance1, min_distance2);
			if (min_distance1 > 0) {
				// Push point apart in perpendicular direction (closer).
				collision.normal = -dir.Tangent().Normalize();
				collision.penetration = collision.normal * capsule_radius;
			} else {
				// Push point apart in parallel direction (closer).
				collision.normal = -dir.Normalize();
				collision.penetration = collision.normal * (math::Sqrt(min_distance1) + capsule_radius);
			}
		}
	} else {
		// Point is within capsule but not on centerline.
		const S distance{ math::Sqrt(distance_squared) };
		// TODO: Check for division by zero?
		collision.normal = vector / distance;
		// Find the amount by which circles overlap.
		collision.penetration = collision.normal * (distance - static_cast<S>(capsule_radius));
	}
	return collision;
}

} // namespace fixed

} // namespace collision

} // namespace ptgn