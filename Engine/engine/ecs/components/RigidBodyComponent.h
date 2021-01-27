#pragma once

#include "Component.h"

#include "physics/RigidBody.h"
#include "physics/Body.h"

struct RigidBodyComponent {
	RigidBody rigid_body;
	Body* body = nullptr;
	~RigidBodyComponent() {
		delete body;
	}
	RigidBodyComponent(Body* body) : body{ body } {}
	RigidBodyComponent(RigidBody rigid_body = {}) : rigid_body{ rigid_body } {
		Init();
	}
	void Init() {
		rigid_body.Init();
		//LOG("Calculated terminal velocity for " << entity.getID() << " : " << rigidBody.terminalVelocity);
	}
};