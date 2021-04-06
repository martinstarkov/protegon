#pragma once

#include "math/Vector2.h"

struct Body;

// http://gamedev.tutsplus.com/tutorials/implementation/create-custom-2d-physics-engine-aabb-circle-impulse-resolution/
struct Manifold {
    Manifold(Body* a, Body* b): A(a), B(b) {}
    ~Manifold() = default;

    void Solve();                 // Generate contact information
    void Initialize(const V2_double& gravity, double dt);            // Precalculations for impulse solving
    void ApplyImpulse();          // Solve impulse and apply
    void PositionalCorrection();  // Naive correction of positional penetration
    void InfiniteMassCorrection();

    Body* A = nullptr;
    Body* B = nullptr;

    double penetration{ 0 };     // Depth of penetration from collision
    V2_double normal{ 0, 0 };    // From A to B
    V2_double contacts[2];     // Points of contact during collision
    std::uint32_t contact_count{ 0 }; // Number of contacts that occured during collision
    double e{ 0 };               // Mixed restitution
    double df{ 0 };              // Mixed dynamic friction
    double sf{ 0 };              // Mixed static friction
};