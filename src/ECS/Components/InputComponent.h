#pragma once

#include "Component.h"

// TODO: Key specific data (WASD vs Arrow keys)

struct InputComponent : public Component<InputComponent> {
	InputComponent() = default;
};

// json serialization
inline void to_json(nlohmann::json& j, const InputComponent& o) {
	// TODO: j["keys"] = o.keys;
}

inline void from_json(const nlohmann::json& j, InputComponent& o) {
	o = InputComponent();
	// TODO: j.at("keys").get<Keys>() into constructor
}