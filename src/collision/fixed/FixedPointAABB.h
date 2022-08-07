#pragma once

#include "math/Vector2.h"
#include "collision/fixed/FixedAABBAABB.h"
#include "collision/fixed/FixedCollision.h"

namespace ptgn {

namespace collision {

namespace fixed {

// Static collision check between a point and an aabb with collision information.
template <typename T, typename S = double,
	std::enable_if_t<std::is_floating_point_v<S>, bool> = true>
inline Collision<S> PointvsAABB(const math::Vector2<T>& point,
						        const math::Vector2<T>& aabb_position,
								const math::Vector2<T>& aabb_size) {
	return AABBvsAABB(point, math::Vector2<T>{ 0, 0 }, aabb_position, aabb_size);
}

} // namespace fixed

} // namespace collision

} // namespace ptgn