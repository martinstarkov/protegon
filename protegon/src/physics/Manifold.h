#pragma once

#include "math/Vector2.h"

namespace engine {

struct Manifold {
    Manifold() = default;
    // Points of contact during collision.
    V2_double contact_point;
    // Normal to collision point from A to B.
    V2_double normal;
    // Depth of collision penetration.
    V2_double penetration;
};

} // namespace engine