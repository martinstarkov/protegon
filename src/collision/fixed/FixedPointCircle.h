#pragma once

#include "math/Vector2.h"
#include "collision/fixed/FixedCollision.h"
#include "collision/fixed/FixedCircleCircle.h"

namespace ptgn {

namespace collision {

namespace fixed {

// Static collision check between a circle and a point with collision information.
template <typename T, typename S = double,
    std::enable_if_t<std::is_floating_point_v<S>, bool> = true>
inline Collision<S> PointvsCircle(const math::Vector2<T>& point,
                                  const math::Vector2<T>& circle_position,
                                  const T circle_radius) {
    return CirclevsCircle(point, static_cast<T>(0), circle_position, circle_radius);
}

} // namespace fixed

} // namespace collision

} // namespace ptgn