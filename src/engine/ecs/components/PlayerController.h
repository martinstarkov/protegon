#pragma once

#include "Component.h"

#include <engine/utils/Vector2.h>

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

// json serialization
inline void to_json(nlohmann::json& j, const PlayerController& o) {
	j["input_acceleration"] = o.input_acceleration;
}

inline void from_json(const nlohmann::json& j, PlayerController& o) {
	if (j.find("input_acceleration") != j.end()) {
		o.input_acceleration = j.at("input_acceleration").get<V2_double>();
	}
	o.Init();
}