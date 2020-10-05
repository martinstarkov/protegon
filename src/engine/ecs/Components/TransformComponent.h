#pragma once

#include "Component.h"

#include <engine/utils/Vector2.h>

struct TransformComponent {
	V2_double position;
	V2_double original_position;
	TransformComponent(V2_double position = {}) : position{ position } {
		Init();
	}
	void ResetPosition() {
		position = original_position;
	}
	void Init() {
		original_position = position;
	}
};

// json serialization
inline void to_json(nlohmann::json& j, const TransformComponent& o) {
	j["position"] = o.position;
	/*j["scale"] = o.scale;
	j["rotation"] = o.rotation;*/
}

inline void from_json(const nlohmann::json& j, TransformComponent& o) {
	if (j.find("position") != j.end()) {
		o.position = j.at("position").get<V2_double>();
	}
	/*if (j.find("scale") != j.end()) {
		o.scale = j.at("scale").get<double>();
	}
	if (j.find("rotation") != j.end()) {
		o.rotation = j.at("rotation").get<double>();
	}*/
	o.Init();
}