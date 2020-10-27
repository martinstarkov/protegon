#pragma once

#include "Component.h"

#include "utils/Vector2.h"

// TODO: Possibly add a maxAcceleration? (for terminalVelocity calculation)
// CONSIDER: Move inputAcceleration / maxAcceleration into another component?

struct PlayerController {
	V2_double input_acceleration;
	PlayerController(V2_double input_acceleration = { 0.0, 0.0 }) : input_acceleration{ input_acceleration } {
		Init();
	}
	// might be useful later
	void Init() {}
};