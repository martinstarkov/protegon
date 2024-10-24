#include "components/rigid_body.h"

#include "core/game.h"
#include "math/vector2.h"
#include "physics/physics.h"

namespace ptgn {

void RigidBody::Update() {
	float dt{ game.physics.dt() };
	velocity += gravity * game.physics.GetGravity() * dt;
	velocity *= 1.0f / (1.0f + drag * dt);
	// Or alternatively: velocity *= std::clamp(1.0f - drag * dt, 0.0f, 1.0f);
	if (max_velocity != -1.0f && velocity.MagnitudeSquared() > max_velocity * max_velocity) {
		velocity = velocity.Normalized() * max_velocity;
	}
}

void RigidBody::AddAcceleration(const V2_float& acceleration) {
	velocity += acceleration * game.physics.dt();
}

void RigidBody::AddImpulse(const V2_float& impulse) {
	velocity += impulse;
}

} // namespace ptgn