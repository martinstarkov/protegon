#pragma once

#include "Component.h"

#include "../../Vec2D.h"

struct TransformComponent : public Component<TransformComponent> {
	Vec2D position; // 8
	double scale; // 4
	double rotation; // 4
	TransformComponent(Vec2D position = Vec2D(), double scale = 1.0, double rotation = 0.0) : position(position), rotation(rotation), scale(scale) {}
};

// json serialization
inline void to_json(nlohmann::json& j, const TransformComponent& o) {
	j["position"] = o.position;
	j["scale"] = o.scale;
	j["rotation"] = o.rotation;
}

inline void from_json(const nlohmann::json& j, TransformComponent& o) {
	o = TransformComponent(
		j.at("position").get<Vec2D>(),
		j.at("scale").get<double>(),
		j.at("rotation").get<double>()
	);
}