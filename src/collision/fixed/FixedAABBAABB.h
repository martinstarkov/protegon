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
    const S direction_x = other_aabb_position.x - aabb_position.x + other_aabb_size.x / 2.0f - aabb_size.x / 2.0f;
    const S penetration_x = (aabb_size.x + other_aabb_size.x) / 2.0f - math::FastAbs(direction_x);
    if (penetration_x < 0 || math::Compare(penetration_x, 0)) {
        return collision;
    }
    const S direction_y = other_aabb_position.y - aabb_position.y + other_aabb_size.y / 2.0f - aabb_size.y / 2.0f;
    const S penetration_y = (aabb_size.y + other_aabb_size.y) / 2.0f - math::FastAbs(direction_y);
    if (penetration_y < 0 || math::Compare(penetration_y, 0)) {
        return collision;
    }

    collision.SetOccured();

    // Edge case where aabb centers are in the same location, choose arbitrary normal to resolve this.
    if (math::Compare(direction_x, 0) && math::Compare(direction_y, 0)) {
        collision.normal.y = -1;
        collision.penetration = collision.normal * (aabb_size.y + other_aabb_size.y) / 2.0f;
    } else if (penetration_x < penetration_y) {
        const S sign_x = math::Sign(direction_x);
        collision.normal.x = -sign_x;
        collision.penetration = collision.normal * math::FastAbs(penetration_x);
    } else {
        const S sign_y = math::Sign(direction_y);
        collision.normal.y = -sign_y;
        collision.penetration = collision.normal * math::FastAbs(penetration_y);
    }
    //collision.point = aabb_position + collision.normal * collision.penetration;
    return collision;
}

} // namespace fixed

} // namespace collision

} // namespace ptgn