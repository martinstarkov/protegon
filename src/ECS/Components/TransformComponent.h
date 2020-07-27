#pragma once

#include "Component.h"

#include "../../Vec2D.h"

struct TransformComponent : public Component<TransformComponent> {
	Vec2D position;
	Vec2D originalPosition;
	double scale;
	double rotation;
	TransformComponent(Vec2D position = Vec2D(), double scale = 1.0, double rotation = 0.0) : position(position), rotation(rotation), scale(scale) {
		init();
	}

	void init() {
		originalPosition = position;
	}
};

// json serialization
inline void to_json(nlohmann::json& j, const TransformComponent& o) {
	j["position"] = o.position;
	j["scale"] = o.scale;
	j["rotation"] = o.rotation;
}

inline void from_json(const nlohmann::json& j, TransformComponent& o) {
	if (j.find("position") != j.end()) {
		o.position = j.at("position").get<Vec2D>();
	}
	if (j.find("scale") != j.end()) {
		o.scale = j.at("scale").get<double>();
	}
	if (j.find("rotation") != j.end()) {
		o.rotation = j.at("rotation").get<double>();
	}
	o.init();
}