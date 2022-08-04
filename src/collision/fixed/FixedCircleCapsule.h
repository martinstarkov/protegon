#pragma once

#include "math/Vector2.h"
#include "collision/fixed/FixedCollision.h"
#include "collision/fixed/FixedCapsuleCapsule.h"

// Source: https://steamcdn-a.akamaihd.net/apps/valve/2015/DirkGregorius_Contacts.pdf

namespace ptgn {

namespace collision {

namespace fixed {

// Get the collision information of a circle and a capsule.
// Capsule origin and destination are taken from the edge circle centers.
template <typename S = double, typename T,
	std::enable_if_t<std::is_floating_point_v<S>, bool> = true>
static Collision<S> CirclevsCapsule(const math::Vector2<T>& circle_center,
									const T circle_radius,
									const math::Vector2<T>& capsule_origin,
									const math::Vector2<T>& capsule_destination,
									const T capsule_radius) {
	return CapsulevsCapsule<S>(circle_center, circle_center, circle_radius,
							   capsule_origin, capsule_destination, capsule_radius);
}

} // namespace fixed

} // namespace collision

} // namespace ptgn