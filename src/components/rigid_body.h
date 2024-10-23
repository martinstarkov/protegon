#pragma once

#include "math/vector2.h"

namespace ptgn {

struct RigidBody {
	V2_float velocity;
	V2_float acceleration;
	float max_velocity{ 0.0f };
	float drag{ 0.0f };
	// Positive is down.
	float gravity{ 0.0f };

	void Update();
};

} // namespace ptgn