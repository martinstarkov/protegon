#include "physics/rigid_body.h"

#include "math/vector2.h"
#include "serialization/json.h"

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

RigidBody::RigidBody(float max_speed, float drag, float gravity, bool immovable) :
	max_speed{ max_speed }, drag{ drag }, gravity{ gravity }, immovable{ immovable } {}

void RigidBody::AddAcceleration(const V2_float& acceleration, float dt) {
	velocity += acceleration * dt;
}

void RigidBody::AddImpulse(const V2_float& impulse) {
	velocity += impulse;
}

void to_json(json& j, const RigidBody& rb) {
	j = json{ { "velocity", rb.velocity },
			  { "max_speed", rb.max_speed },
			  { "drag", rb.drag },
			  { "gravity", rb.gravity },
			  { "immovable", rb.immovable } };
}

void from_json(const json& j, RigidBody& rb) {
	if (j.contains("velocity")) {
		j.at("velocity").get_to(rb.velocity);
	} else {
		rb.velocity = {};
	}
	if (j.contains("max_speed")) {
		j.at("max_speed").get_to(rb.max_speed);
		PTGN_ASSERT(rb.max_speed >= 0.0f, "Max speed must be a positive number or -1 to omit it");
	} else {
		rb.max_speed = -1.0f;
	}
	if (j.contains("drag")) {
		j.at("drag").get_to(rb.drag);
	} else {
		rb.drag = 0.0f;
	}
	if (j.contains("gravity")) {
		j.at("gravity").get_to(rb.gravity);
	} else {
		rb.gravity = 0.0f;
	}
	if (j.contains("immovable")) {
		j.at("immovable").get_to(rb.immovable);
	} else {
		rb.immovable = false;
	}
}

} // namespace ptgn