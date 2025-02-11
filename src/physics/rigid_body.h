#pragma once

#include "math/vector2.h"

namespace ptgn {

struct RigidBody {
	V2_float velocity;
	// -1 means no enforcement of maximum velocity.
	float max_velocity{ -1.0f };
	float drag{ 0.0f };
	// Gravity relative to game.physics.GetGravity().
	float gravity{ 0.0f };
	bool immovable{ false };

	// vel += accel * dt
	// @param dt Unit: seconds.
	void AddAcceleration(const V2_float& acceleration, float dt);

	// vel += impulse
	void AddImpulse(const V2_float& impulse);

	// @param dt Unit: seconds.
	void Update(const V2_float& physics_gravity, float dt);
};

} // namespace ptgn