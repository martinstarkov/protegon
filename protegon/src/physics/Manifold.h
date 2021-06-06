#pragma once

#include "math/Vector2.h"

namespace ptgn {

struct Manifold {
    Manifold() = default;
    ~Manifold() = default;

    // Returns true if a collision is registered in the manifold.
    // This means the collision normal vector is non-zero.
    bool CollisionOccured() const {
        return !normal.IsZero();
    }

    // Points of contact during collision.
    V2_double contact_point;

    // Normal to collision point from A to B.
    V2_double normal;

    // Depth of collision penetration.
    V2_double penetration;
};

} // namespace ptgn