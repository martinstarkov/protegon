#pragma once

#include "Component.h"

#include "../../Direction.h"

struct DirectionComponent : public Component<DirectionComponent> {
	Direction direction;
	Direction previousDirection;
	DirectionComponent(Direction direction = Direction::DOWN) : direction(direction), previousDirection(direction) {}
};

// json serialization
inline void to_json(nlohmann::json& j, const DirectionComponent& o) {
	j["direction"] = o.direction;
}

inline void from_json(const nlohmann::json& j, DirectionComponent& o) {
	o = DirectionComponent(
		j.at("direction").get<Direction>()
	);
}