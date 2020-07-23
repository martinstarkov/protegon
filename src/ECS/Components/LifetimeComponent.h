#pragma once

#include "Component.h"

struct LifetimeComponent : public Component<LifetimeComponent> {
	double lifetime;
	bool isDying;
	LifetimeComponent(double lifetime = 1.0, bool isDying = true) : lifetime(lifetime), isDying(isDying) {}
};

// json serialization
inline void to_json(nlohmann::json& j, const LifetimeComponent& o) {
	j["lifetime"] = o.lifetime;
	j["isDying"] = o.isDying;
}

inline void from_json(const nlohmann::json& j, LifetimeComponent& o) {
	o = LifetimeComponent(
		j.at("lifetime").get<double>(),
		j.at("isDying").get<bool>()
	);
}