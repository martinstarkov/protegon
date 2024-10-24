#pragma once

#include "math/vector2.h"

namespace ptgn {

struct RigidBody {
	V2_float velocity;
	// -1.0f means no enforcement of maximum velocity.
	float max_velocity{ -1.0f };
	float drag{ 0.0f };
	// Gravity relative to game.physics.GetGravity().
	float gravity{ 1.0f };

	// vel += accel * dt
	void AddAcceleration(const V2_float& acceleration);

	// vel += impulse
	void AddImpulse(const V2_float& impulse);

	void Update();
};

} // namespace ptgn