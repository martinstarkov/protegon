#pragma once

#include "Component.h"

#include <engine/renderer/Color.h>

// TODO: Add color serialization

struct RenderComponent {
	engine::Color color;
	engine::Color original_color;
	RenderComponent(engine::Color color = { 0, 0, 0, 0 }) : color{ color } {
		Init();
	}
	void Init() {
		original_color = color;
	}
	void ResetColor() {
		color = original_color;
	}
};

//// json serialization
//inline void to_json(nlohmann::json& j, const RenderComponent& o) {
//	j["color"] = o.color;
//}
//
//inline void from_json(const nlohmann::json& j, RenderComponent& o) {
//	if (j.find("color") != j.end()) {
//		j.at("color").get<SDL_Color>();
//	}
//  o.Init();
//}
