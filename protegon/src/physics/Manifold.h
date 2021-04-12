#pragma once

#include <array>

#include "Transform.h"
#include "RigidBody.h"

namespace engine {

struct Manifold {
    Manifold() = default;
    V2_double contact_point; // Points of contact during collision.
    V2_double normal; // Normal to collision point from A to B.
    V2_double penetration; // Depth of collision penetration.
};

} // namespace engine