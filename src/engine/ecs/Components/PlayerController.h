#pragma once

#include "Component.h"

#include <Vec2D.h>

// TODO: Possibly add a maxAcceleration? (for terminalVelocity calculation)
// CONSIDER: Move inputAcceleration / maxAcceleration into another component?

struct PlayerController {
	Vec2D inputAcceleration;
	PlayerController(Vec2D input_acceleration = {}) : inputAcceleration{ input_acceleration } {
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
		o.inputAcceleration = j.at("inputAcceleration").get<Vec2D>();
	}
	o.Init();
}