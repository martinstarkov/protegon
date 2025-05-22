#include "physics/rigid_body.h"

#include "math/vector2.h"

namespace ptgn {

void RigidBody::Update(const V2_float& physics_gravity, float dt) {
	velocity += gravity * physics_gravity * dt;
	velocity *= 1.0f / (1.0f + drag * dt);
	// Or alternatively: velocity *= std::clamp(1.0f - drag * dt, 0.0f, 1.0f);
	if (max_speed != -1.0f) {
		PTGN_ASSERT(max_speed >= 0.0f, "Max speed must be a positive number or -1 to omit it");
		velocity = Clamp(velocity, -max_speed, max_speed);
	}
}

RigidBody::RigidBody(float rb_max_speed, float rb_drag, float rb_gravity, bool rb_immovable) :
	max_speed{ rb_max_speed }, drag{ rb_drag }, gravity{ rb_gravity }, immovable{ rb_immovable } {}

void RigidBody::AddAcceleration(const V2_float& acceleration, float dt) {
	velocity += acceleration * dt;
}

void RigidBody::AddImpulse(const V2_float& impulse) {
	velocity += impulse;
}

} // namespace ptgn