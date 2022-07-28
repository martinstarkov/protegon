#pragma once

#include "math/Vector2.h"
#include "collision/overlap/OverlapCapsuleCapsule.h"

namespace ptgn {

namespace collision {

namespace overlap {

// Check if a line and a capsule overlap.
// Capsule origin and destination are taken from the edge circle centers.
template <typename T, typename S = double,
	std::enable_if_t<std::is_floating_point_v<S>, bool> = true>
inline bool LinevsCapsule(const math::Vector2<T>& line_origin,
						  const math::Vector2<T>& line_destination,
						  const math::Vector2<T>& capsule_origin,
						  const math::Vector2<T>& capsule_destination,
						  const T capsule_radius) {
	return CapsulevsCapsule(line_origin, line_destination, 
							static_cast<T>(0), capsule_origin, 
							capsule_destination, capsule_radius);
}

} // namespace overlap

} // namespace collision

} // namespace ptgn