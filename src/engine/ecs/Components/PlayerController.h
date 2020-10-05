#pragma once

#include "Component.h"

#include <engine/utils/Vector2.h>

// TODO: Possibly add a maxAcceleration? (for terminalVelocity calculation)
// CONSIDER: Move inputAcceleration / maxAcceleration into another component?

struct PlayerController {
	V2_double inputAcceleration;
	PlayerController(V2_double input_acceleration = {}) : inputAcceleration{ input_acceleration } {
		Init();
	}
	// might be useful later
	void Init() {}
};

// json serialization
inline void to_json(nlohmann::json& j, const PlayerController& o) {
	j["inputAcceleration"] = o.inputAcceleration;
}

inline void from_json(const nlohmann::json& j, PlayerController& o) {
	if (j.find("inputAcceleration") != j.end()) {
		o.inputAcceleration = j.at("inputAcceleration").get<V2_double>();
	}
	o.Init();
}