#pragma once

#include "Component.h"

#include "math/Vector2.h"

// TODO: Possibly add a maxAcceleration? (for terminalVelocity calculation)
// CONSIDER: Move inputAcceleration / maxAcceleration into another component?

struct PlayerController {
	V2_double input_acceleration;
	PlayerController() = default;
	PlayerController(const V2_double& input_acceleration) : 
		input_acceleration{ input_acceleration } {
		Init();
	}
	// might be useful later
	void Init() {}
};