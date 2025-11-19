#pragma once

#include "math/vector2.h"
#include "serialization/json/serializable.h"

namespace ptgn {

class Entity;

struct RigidBody {
	RigidBody() = default;
	RigidBody(float max_speed, float drag, float gravity, bool immovable);

	// vel += accel * dt
	// @param dt Unit: seconds.
	void AddAcceleration(const V2_float& acceleration, float dt);

	// angular_vel += angular_accel * dt
	// @param dt Unit: seconds.
	void AddAngularAcceleration(float angular_acceleration, float dt);

	// vel += impulse
	void AddImpulse(const V2_float& impulse);

	// angular_vel += angular_impulse
	void AddAngularImpulse(float angular_impulse);

	// @param dt Unit: seconds.
	void Update(const V2_float& physics_gravity, float dt);

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(
		RigidBody, max_speed, max_angular_speed, drag, angular_drag, gravity, immovable, velocity,
		angular_velocity
	)

	// -1 means no enforcement of maximum speed.
	float max_speed{ -1.0f };
	float max_angular_speed{ -1.0f };
	float drag{ 0.0f };
	float angular_drag{ 0.0f };
	// Gravity relative to Application::Get().physics.GetGravity().
	float gravity{ 0.0f };
	bool immovable{ false };
	V2_float velocity;
	float angular_velocity{ 0.0f };
};

[[nodiscard]] bool IsImmovable(const Entity& entity, bool check_parents = true);

} // namespace ptgn