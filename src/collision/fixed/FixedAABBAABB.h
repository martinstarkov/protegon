#pragma once

#include "math/Vector2.h"
#include "math/Math.h"
#include "collision/fixed/FixedCollision.h"

namespace ptgn {

namespace collision {

namespace fixed {

// Static collision check between two aabbs with collision information.
template <typename T, typename S = double,
	std::enable_if_t<std::is_floating_point_v<S>, bool> = true>
static Collision<S> AABBvsAABB(const math::Vector2<T>& aabb_position,
							   const math::Vector2<T>& aabb_size,
							   const math::Vector2<T>& other_aabb_position,
							   const math::Vector2<T>& other_aabb_size) {
	Collision<S> collision;
    const auto direction_x = other_aabb_position.x - aabb_position.x + other_aabb_size.x / static_cast<S>(2) - aabb_size.x / static_cast<S>(2);
    const auto penetration_x = (aabb_size.x + other_aabb_size.x) / static_cast<S>(2) - math::Abs(direction_x);
    if (penetration_x < 0 || math::Compare(penetration_x, static_cast<S>(0))) {
        return collision;
    }
    const auto direction_y = other_aabb_position.y - aabb_position.y + other_aabb_size.y / static_cast<S>(2) - aabb_size.y / static_cast<S>(2);
    const auto penetration_y = (aabb_size.y + other_aabb_size.y) / static_cast<S>(2) - math::Abs(direction_y);
    if (penetration_y < 0 || math::Compare(penetration_y, static_cast<S>(0))) {
        return collision;
    }

    collision.SetOccured();

    // Edge case where aabb centers are in the same location, choose arbitrary normal to resolve this.
    if (math::Compare(direction_x, static_cast<S>(0)) && math::Compare(direction_y, static_cast<S>(0))) {
        collision.penetration = (aabb_size.y + other_aabb_size.y) / static_cast<S>(2);
        collision.normal.y = static_cast<S>(-1);
    } else if (penetration_x < penetration_y) {
        const auto sx = math::Sign(direction_x);
        collision.penetration = math::Abs(penetration_x);
        collision.normal.x = -sx;
    } else {
        const auto sy = math::Sign(direction_y);
        collision.penetration = math::Abs(penetration_y);
        collision.normal.y = -sy;
    }
    collision.point = aabb_position + collision.normal * collision.penetration;
    return collision;
}

} // namespace fixed

} // namespace collision

} // namespace ptgn