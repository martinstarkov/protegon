#pragma once

#include "Component.h"

#include "../../Vec2D.h"

#define DEFAULT_GRAVITY 0.001

struct GravityComponent : public Component<GravityComponent> {
	double g;
	Vec2D direction;
	GravityComponent(double g = DEFAULT_GRAVITY, Vec2D direction = Vec2D(0, 1)) : g(g), direction(direction) {}
};

// json serialization
inline void to_json(nlohmann::json& j, const GravityComponent& o) {
	j["g"] = o.g;
	j["direction"] = o.direction;
}

inline void from_json(const nlohmann::json& j, GravityComponent& o) {
	o = GravityComponent(
		j.at("g").get<double>(),
		j.at("direction").get<Vec2D>()
	);
}