#pragma once

#include "Component.h"

#include "SDL.h"

// TODO: Add color serialization

struct RenderComponent : public Component<RenderComponent> {
	SDL_Color color;
	RenderComponent(SDL_Color color = { 0, 0, 0, 255 }) : color(color) {}
};

// json serialization
inline void to_json(nlohmann::json& j, const RenderComponent& o) {
	// j["variable"] = o.variable;
}

inline void from_json(const nlohmann::json& j, RenderComponent& o) {
	// j.at("variable").get<type>()
}
