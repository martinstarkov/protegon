#pragma once

#include "math/vector2.h"
#include "serialization/serializable.h"

namespace ptgn {

struct RigidBody {
	RigidBody() = default;
	RigidBody(float max_speed, float drag, float gravity, bool immovable);

	// vel += accel * dt
	// @param dt Unit: seconds.
	void AddAcceleration(const V2_float& acceleration, float dt);

	// vel += impulse
	void AddImpulse(const V2_float& impulse);

	// @param dt Unit: seconds.
	void Update(const V2_float& physics_gravity, float dt);

	PTGN_SERIALIZER_REGISTER(RigidBody, max_speed, drag, gravity, immovable, velocity)

	// -1 means no enforcement of maximum speed.
	float max_speed{ -1.0f };
	float drag{ 0.0f };
	// Gravity relative to game.physics.GetGravity().
	float gravity{ 0.0f };
	bool immovable{ false };
	V2_float velocity;
};

} // namespace ptgn