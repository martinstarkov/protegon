#pragma once

#include "math/Vector2.h"
#include "collision/overlap/OverlapCircleCapsule.h"

namespace ptgn {

namespace collision {

namespace overlap {

// Check if a point and a capsule overlap.
// Capsule origin and destination are taken from the edge circle centers.
template <typename T>
inline bool PointvsCapsule(const math::Vector2<T>& point,
						   const math::Vector2<T>& capsule_origin,
						   const math::Vector2<T>& capsule_destination,
						   const T capsule_radius) {
	return CirclevsCapsule(point, static_cast<T>(0), capsule_origin, capsule_destination, capsule_radius);
}

} // namespace overlap

} // namespace collision

} // namespace ptgn