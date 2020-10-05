#pragma once

#include "Component.h"

#include <Direction.h>

struct DirectionComponent {
	Direction direction;
	Direction previous_direction;
	DirectionComponent(Direction direction = Direction::DOWN) : direction{ direction } {
		Init();
	}
	void Init() {
		previous_direction = direction;
	}
};

// json serialization
inline void to_json(nlohmann::json& j, const DirectionComponent& o) {
	j["direction"] = o.direction;
}

inline void from_json(const nlohmann::json& j, DirectionComponent& o) {
	if (j.find("direction") != j.end()) {
		o.direction = j.at("direction").get<Direction>();
	}
	o.Init();
}