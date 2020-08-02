#pragma once

#include "Component.h"

struct LifetimeComponent : public Component<LifetimeComponent> {
	double lifetime; // seconds
	bool isDying; // lose lifetime every frame
	LifetimeComponent(double lifetime = 1.0, bool isDying = true) : lifetime(lifetime), isDying(isDying) {}
};

// json serialization
inline void to_json(nlohmann::json& j, const LifetimeComponent& o) {
	j["lifetime"] = o.lifetime;
	j["isDying"] = o.isDying;
}

inline void from_json(const nlohmann::json& j, LifetimeComponent& o) {
	if (j.find("lifetime") != j.end()) {
		o.lifetime = j.at("lifetime").get<double>();
	}
	if (j.find("isDying") != j.end()) {
		o.isDying = j.at("isDying").get<bool>();
	}
}