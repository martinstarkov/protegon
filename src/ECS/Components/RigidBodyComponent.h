#pragma once

#include "Component.h"

#include "../../RigidBody.h"

// TODO: Add init() method to RigidBodyComponent, put rigidBody.init() inside it and do o.init() outside deserialize if statement

struct RigidBodyComponent : public Component<RigidBodyComponent> {
	RigidBody rigidBody;
	RigidBodyComponent(RigidBody rigidBody = RigidBody()) : rigidBody(rigidBody) {}
};

// json serialization
inline void to_json(nlohmann::json& j, const RigidBodyComponent& o) {
	j["rigidBody"] = o.rigidBody;
}

inline void from_json(const nlohmann::json& j, RigidBodyComponent& o) {
	if (j.find("rigidBody") != j.end()) {
		o.rigidBody = j.at("rigidBody").get<RigidBody>();
		o.rigidBody.init();
	}
}
