#pragma once

#include "Component.h"

#include "../../Vec2D.h"

// TODO: Possible maxSpeed?
// CONSIDER: Move speed elsewhere?

struct PlayerController : public Component<PlayerController> {
	Vec2D inputAcceleration;
	PlayerController(Vec2D inputAcceleration = Vec2D()) : inputAcceleration(inputAcceleration) {}
};

// json serialization
inline void to_json(nlohmann::json& j, const PlayerController& o) {
	j["inputAcceleration"] = o.inputAcceleration;
}

inline void from_json(const nlohmann::json& j, PlayerController& o) {
	if (j.find("inputAcceleration") != j.end()) {
		o.inputAcceleration = j.at("inputAcceleration").get<Vec2D>();
	}
}