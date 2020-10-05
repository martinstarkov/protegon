#pragma once

#include "Component.h"

struct LifetimeComponent {
	double lifetime; // seconds
	bool is_dying; // lose lifetime every frame
	LifetimeComponent(double lifetime = 1.0, bool is_dying = true) : lifetime{ lifetime }, is_dying{ is_dying } {}
};

// json serialization
inline void to_json(nlohmann::json& j, const LifetimeComponent& o) {
	j["lifetime"] = o.lifetime;
	j["is_dying"] = o.is_dying;
}

inline void from_json(const nlohmann::json& j, LifetimeComponent& o) {
	if (j.find("lifetime") != j.end()) {
		o.lifetime = j.at("lifetime").get<double>();
	}
	if (j.find("is_dying") != j.end()) {
		o.is_dying = j.at("is_dying").get<bool>();
	}
}