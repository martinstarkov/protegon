#pragma once

#include "math/Vector2.h"

namespace ptgn {

namespace collision {

namespace fixed {

template <typename T,
    std::enable_if_t<std::is_floating_point_v<T>, bool> = true>
struct Collision {
    Collision() = default;
    ~Collision() = default;
    bool Occured() const {
        return occured_;
    }
    // Point at which collision occurs (where the collision plane is created).
    math::Vector2<T> point;
    // Normal vector to the collision plane.
    math::Vector2<T> normal;
    // Penetration of objects into each other along the collision normal.
    T penetration;
    void SetOccured() {
        occured_ = true;
    }
private:
    bool occured_{ false };
};

} // namespace fixed

} // namespace collision

} // namespace ptgn