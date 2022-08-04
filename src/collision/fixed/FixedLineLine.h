#pragma once

#include "math/Vector2.h"
#include "collision/fixed/FixedCollision.h"
#include "collision/fixed/FixedCapsuleCapsule.h"

namespace ptgn {

namespace collision {

namespace fixed {

// Get the collision information of two lines.
template <typename S = double, typename T,
	std::enable_if_t<std::is_floating_point_v<S>, bool> = true>
static Collision<S> LinevsLine(const math::Vector2<T>& line_origin,
									  const math::Vector2<T>& line_destination,
									  const math::Vector2<T>& other_line_origin,
									  const math::Vector2<T>& other_line_destination) {
	return CapsulevsCapsule<S>(line_origin, line_destination, static_cast<T>(0), other_line_origin, other_line_destination, static_cast<T>(0));
}

} // namespace fixed

} // namespace collision

} // namespace ptgn