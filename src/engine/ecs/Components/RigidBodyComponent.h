#pragma once

#include "Component.h"

#include <engine/physics/RigidBody.h>

struct RigidBodyComponent {
	RigidBody rigid_body;
	RigidBodyComponent(RigidBody rigid_body = {}) : rigid_body{ rigid_body } {
		Init();
	}
	void Init() {
		rigid_body.Init();
		//LOG("Calculated terminal velocity for " << entity.getID() << " : " << rigidBody.terminalVelocity);
	}
};

// json serialization
inline void to_json(nlohmann::json& j, const RigidBodyComponent& o) {
	j["rigid_body"] = o.rigid_body;
}

inline void from_json(const nlohmann::json& j, RigidBodyComponent& o) {
	if (j.find("rigid_body") != j.end()) {
		o.rigid_body = j.at("rigid_body").get<RigidBody>();
	}
	o.Init();
}
