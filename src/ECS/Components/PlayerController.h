#pragma once

#include "Component.h"

#include "../../Vec2D.h"

// TODO: Possible maxSpeed?
// CONSIDER: Move speed elsewhere?

struct PlayerController : public Component<PlayerController> {
	Vec2D speed;
	Vec2D originalSpeed;
	PlayerController(Vec2D speed = Vec2D()) : speed(speed), originalSpeed(speed) {}
};

// json serialization
inline void to_json(nlohmann::json& j, const PlayerController& o) {
	j["speed"] = o.speed;
}

inline void from_json(const nlohmann::json& j, PlayerController& o) {
	o = PlayerController(
		j.at("speed").get<Vec2D>()
	);
}