#pragma once

#include "math/Vector2.h"
#include "collision/fixed/FixedCapsuleCapsule.h"
#include "collision/fixed/FixedCollision.h"

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
	return CapsulevsCapsule<S>(point, point, static_cast<T>(0), capsule_origin, capsule_destination, capsule_radius);
}

} // namespace fixed

} // namespace collision

} // namespace ptgn