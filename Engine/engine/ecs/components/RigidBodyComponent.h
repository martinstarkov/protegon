#pragma once

#include "Component.h"

#include "physics/RigidBody.h"

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