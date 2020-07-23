#pragma once

#include "Component.h"

struct RenderComponent : public Component<RenderComponent> {
	RenderComponent() = default;
};

// json serialization
inline void to_json(nlohmann::json& j, const RenderComponent& o) {
	// j["variable"] = o.variable;
}

inline void from_json(const nlohmann::json& j, RenderComponent& o) {
	o = RenderComponent();
	// j.at("variable").get<type>()
}