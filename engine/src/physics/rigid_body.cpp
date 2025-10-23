#include "physics/rigid_body.h"

#include "core/ecs/entity.h"
#include "core/ecs/entity_hierarchy.h"
#include "math/vector2.h"

namespace ptgn {

void RigidBody::Update(const V2_float& physics_gravity, float dt) {
	velocity += gravity * physics_gravity * dt;
	velocity *= 1.0f / (1.0f + drag * dt);
	// Or alternatively: velocity *= std::clamp(1.0f - drag * dt, 0.0f, 1.0f);
	angular_velocity *= 1.0f / (1.0f + angular_drag * dt);
	if (max_speed != -1.0f) {
		PTGN_ASSERT(max_speed >= 0.0f, "Max speed must be a positive number or -1 to omit it");
		velocity = Clamp(velocity, -max_speed, max_speed);
	}
	if (max_angular_speed != -1.0f) {
		PTGN_ASSERT(
			max_angular_speed >= 0.0f,
			"Max angular speed must be a positive number or -1 to omit it"
		);
		angular_velocity = std::clamp(angular_velocity, -max_angular_speed, max_angular_speed);
	}
}

RigidBody::RigidBody(float rb_max_speed, float rb_drag, float rb_gravity, bool rb_immovable) :
	max_speed{ rb_max_speed }, drag{ rb_drag }, gravity{ rb_gravity }, immovable{ rb_immovable } {}

void RigidBody::AddAcceleration(const V2_float& acceleration, float dt) {
	velocity += acceleration * dt;
}

void RigidBody::AddAngularAcceleration(float angular_acceleration, float dt) {
	angular_velocity += angular_acceleration * dt;
}

void RigidBody::AddImpulse(const V2_float& impulse) {
	velocity += impulse;
}

void RigidBody::AddAngularImpulse(float angular_impulse) {
	angular_velocity += angular_impulse;
}

bool IsImmovable(const Entity& entity, bool check_parents) {
	if (entity.Has<RigidBody>() && entity.Get<RigidBody>().immovable) {
		return true;
	}
	if (check_parents && HasParent(entity)) {
		Entity parent{ GetParent(entity) };
		return IsImmovable(parent, check_parents);
	}
	return false;
}

} // namespace ptgn