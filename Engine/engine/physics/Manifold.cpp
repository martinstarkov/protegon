#include "Manifold.h"

#include "Body.h"

#include <algorithm> // std::min

#include "Collision.h"

void Manifold::Solve() {
    switch (B->shape->GetType()) {
        case ShapeType::CIRCLE:
            A->shape->CollisionCheck(this, reinterpret_cast<Circle*>(B->shape));
            break;
        case ShapeType::POLYGON:
            A->shape->CollisionCheck(this, reinterpret_cast<Polygon*>(B->shape));
            break;
        default: break;
    }
    //Dispatch[static_cast<int>(A->shape->GetType())][static_cast<int>(B->shape->GetType())](this, A, B);
}

void Manifold::Initialize(const V2_double& gravity, double dt) {
    // Calculate average restitution
    e = std::min(A->restitution, B->restitution);

    // Calculate static and dynamic friction
    sf = std::sqrt(A->static_friction * A->static_friction);
    df = std::sqrt(A->dynamic_friction * A->dynamic_friction);

    for (std::uint32_t i = 0; i < contact_count; ++i) {
        // Calculate radii from COM to contact
        auto ra = contacts[i] - A->position;
        auto rb = contacts[i] - B->position;

        auto rv = B->velocity + CrossProduct(B->angular_velocity, rb) -
            A->velocity - CrossProduct(A->angular_velocity, ra);


        // Determine if we should perform a resting collision or not
        // The idea is if the only thing moving this object is gravity,
        // then the collision should be performed without any restitution
        if (rv.MagnitudeSquared() < (dt * gravity).MagnitudeSquared() + DBL_EPSILON)
            e = 0.0;
    }
}

// Comparison with tolerance of EPSILON
inline bool Equal(double a, double b) {
    // <= instead of < for NaN comparison safety
    return std::abs(a - b) <= DBL_EPSILON;
}

void Manifold::ApplyImpulse() {
    // Early out and positional correct if both objects have infinite mass
    if (Equal(A->inverse_mass + B->inverse_mass, 0)) {
        InfiniteMassCorrection();
        return;
    }

    for (std::uint32_t i = 0; i < contact_count; ++i) {
        // Calculate radii from COM to contact
        auto ra = contacts[i] - A->position;
        auto rb = contacts[i] - B->position;

        // Relative velocity
        auto rv = B->velocity + CrossProduct(B->angular_velocity, rb) -
            A->velocity - CrossProduct(A->angular_velocity, ra);

        // Relative velocity along the normal
        auto contactVel = rv.DotProduct(normal);

        // Do not resolve if velocities are separating
        if (contactVel > 0)
            return;

        auto raCrossN = ra.CrossProduct(normal);
        auto rbCrossN = rb.CrossProduct(normal);
        auto invMassSum = A->inverse_mass + B->inverse_mass + (raCrossN * raCrossN) * A->inverse_inertia + (rbCrossN * rbCrossN) * B->inverse_inertia;

        // Calculate impulse scalar
        auto j = -(1.0 + e) * contactVel;
        j /= invMassSum;
        j /= (double)contact_count;

        // Apply impulse
        auto impulse = normal * j;
        A->ApplyImpulse(-impulse, ra);
        B->ApplyImpulse(impulse, rb);

        // Friction impulse
        rv = B->velocity + CrossProduct(B->angular_velocity, rb) -
            A->velocity - CrossProduct(A->angular_velocity, ra);

        auto t = rv - (normal * rv.DotProduct(normal));
        t = t.Normalized();

        // j tangent magnitude
        auto jt = -rv.DotProduct(t);
        jt /= invMassSum;
        jt /= (double)contact_count;

        // Don't apply tiny friction impulses
        if (Equal(jt, 0.0))
            return;

        // Coulumb's law
        V2_double tangentImpulse;
        if (std::abs(jt) < j * sf)
            tangentImpulse = t * jt;
        else
            tangentImpulse = t * -j * df;

        // Apply friction impulse
        A->ApplyImpulse(-tangentImpulse, ra);
        B->ApplyImpulse(tangentImpulse, rb);
    }
}

void Manifold::PositionalCorrection() {
    const auto k_slop = 0.01; // 0.05; // Penetration allowance
    const auto percent = 1.01; // 0.4 // Penetration percentage to correct
    auto correction = (std::max(penetration - k_slop, 0.0) / (A->inverse_mass + B->inverse_mass)) * normal * percent;
    A->position -= correction * A->inverse_mass;
    B->position += correction * B->inverse_mass;
}

void Manifold::InfiniteMassCorrection() {
    A->velocity = { 0, 0 };
    B->velocity = { 0, 0 };
}