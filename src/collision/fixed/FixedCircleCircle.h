#pragma once

#include "math/Vector2.h"
#include "math/Math.h"
#include "collision/fixed/FixedCollision.h"

namespace ptgn {

namespace collision {

namespace fixed {

// Static collision check between two circles with collision information.
template <typename T, typename S = double,
    std::enable_if_t<std::is_floating_point_v<S>, bool> = true>
static Collision<S> CirclevsCircle(const math::Vector2<T>& circle_position,
                                    const T circle_radius,
                                    const math::Vector2<T>& other_circle_position,
                                    const T other_circle_radius) {
    static_assert(!tt::is_narrowing_v<T, S>);
    Collision<S> collision;

    const math::Vector2<T> direction{ other_circle_position - circle_position };
    const T distance_squared{ direction.MagnitudeSquared() };
    const T combined_radius{ circle_radius + other_circle_radius };
    const T combined_radius_squared{ combined_radius * combined_radius };

    // Collision did not occur, exit with empty collision.
    if (distance_squared > combined_radius_squared ||
        math::Compare(distance_squared, combined_radius_squared)) {
        return collision;
    }

    collision.SetOccured();

    const S distance{ std::sqrt(distance_squared) };

    // Bias toward selecting first circle for exact overlap edge case.
    if (math::Compare(distance, 0)) {
        // Arbitrary normal chosen upward.
        collision.normal = { 0, -1 };
        collision.penetration = collision.normal * combined_radius;
    } else {
        // Normalise collision vector.
        collision.normal = direction / distance;
        // Find the amount by which circles overlap.
        collision.penetration = collision.normal * (distance - static_cast<S>(combined_radius));
    }
    // Find point of collision from the first circle.
    //collision.point = circle_position + collision.penetration * collision.normal;
    return collision;
}

} // namespace fixed

} // namespace collision

} // namespace ptgn