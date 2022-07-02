#pragma once

#include "math/Vector2.h"

namespace ptgn {

struct Manifold {
    Manifold() = default;
    ~Manifold() = default;

    // Return true if the manifold contains a collision.
    operator bool() {
        return CollisionOccured();
    }

    // Reset manifold to non-collision.
    void Reset() {
        contact_point = {};
        normal = {};
        penetration = {};
    }

    // Returns true if a collision is registered in the manifold.
    // This means the collision normal vector is non-zero.
    bool CollisionOccured() const {
        return !normal.IsZero();
    }

    // Compare collision manifolds.

    bool operator==(const Manifold& o) {
        return contact_point == o.contact_point &&
            normal == o.normal &&
            penetration == o.penetration;
    }

    bool operator!=(const Manifold& o) {
        return !(*this == o);
    }

    // Points of contact during collision.
    V2_double contact_point;

    // Normal to collision point from A to B.
    V2_double normal;

    // Depth of collision penetration.
    V2_double penetration;
};

} // namespace ptgn