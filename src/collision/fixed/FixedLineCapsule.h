#pragma once

#include "math/Vector2.h"
#include "collision/fixed/FixedCollision.h"
#include "collision/fixed/FixedCapsuleCapsule.h"

namespace ptgn {

namespace collision {

namespace fixed {

// Get the collision information of a line and a capsule.
// Capsule origin and destination are taken from the edge circle centers.
template <typename S = double, typename T,
	std::enable_if_t<std::is_floating_point_v<S>, bool> = true>
static Collision<S> LinevsCapsule(const math::Vector2<T>& line_origin,
								  const math::Vector2<T>& line_destination,
								  const math::Vector2<T>& capsule_origin,
								  const math::Vector2<T>& capsule_destination,
								  const T capsule_radius) {
	return CapsulevsCapsule<S>(line_origin, line_destination, 0, capsule_origin, capsule_destination, capsule_radius);
}

} // namespace fixed

} // namespace collision

} // namespace ptgn