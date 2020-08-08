#pragma once

#include "Component.h"

#include "../../RigidBody.h"

struct RigidBodyComponent : public Component<RigidBodyComponent> {
	RigidBody rigidBody;
	RigidBodyComponent(RigidBody rigidBody = RigidBody()) : rigidBody(rigidBody) {
		init();
	}
	void init() {
		rigidBody.init();
		//LOG("Calculated terminal velocity for " << entity.getID() << " : " << rigidBody.terminalVelocity);
	}
};

// json serialization
inline void to_json(nlohmann::json& j, const RigidBodyComponent& o) {
	j["rigidBody"] = o.rigidBody;
}

inline void from_json(const nlohmann::json& j, RigidBodyComponent& o) {
	if (j.find("rigidBody") != j.end()) {
		o.rigidBody = j.at("rigidBody").get<RigidBody>();
	}
	o.init();
}
