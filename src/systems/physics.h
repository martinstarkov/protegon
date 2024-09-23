#pragma once

namespace ptgn {

// RigidBody

// velocity = ApplyForces(velocity);
// velocity *= Mathf.Clamp01(1-drag * dt);
// or
// velocity *= 1 / (1 + drag*dt);
// velocity = ApplyCollisionForces(velocity);
// position += velocity * dt;

// if (velocity.MagnitudeSquared() > max_velocity * max_velocity) {
//     velocity = velocity.Normalized() * max_velocity;
// }

} // namespace ptgn