#pragma once

#include "Component.h"

#include <engine/physics/RigidBody.h>

struct RigidBodyComponent {
	RigidBody rigidBody;
	RigidBodyComponent(RigidBody rigid_body = {}) : rigidBody{ rigid_body } {
		Init();
	}
	void Init() {
		rigidBody.Init();
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
	o.Init();
}
