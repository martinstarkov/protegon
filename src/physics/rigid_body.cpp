#include "physics/rigid_body.h"

#include "math/vector2.h"

namespace ptgn {

void RigidBody::Update(const V2_float& physics_gravity, float dt) {
	velocity += gravity * physics_gravity * dt;
	velocity *= 1.0f / (1.0f + drag * dt);
	// Or alternatively: velocity *= std::clamp(1.0f - drag * dt, 0.0f, 1.0f);
	if (max_velocity != -1.0f && velocity.MagnitudeSquared() > max_velocity * max_velocity) {
		velocity = velocity.Normalized() * max_velocity;
	}
}

void RigidBody::AddAcceleration(const V2_float& acceleration, float dt) {
	velocity += acceleration * dt;
}

void RigidBody::AddImpulse(const V2_float& impulse) {
	velocity += impulse;
}

} // namespace ptgn