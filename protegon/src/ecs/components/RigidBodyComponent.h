#pragma once

#include "Component.h"

#include "physics/RigidBody.h"
#include "physics/Body.h"

struct RigidBodyComponent {
	RigidBody rigid_body;
	Body* body = nullptr;
	RigidBodyComponent(Body* body) : body{ body } {}
	RigidBodyComponent(RigidBody rigid_body = {}) : rigid_body{ rigid_body } {
		Init();
	}
	RigidBodyComponent(const RigidBodyComponent& copy) {
		rigid_body = copy.rigid_body;
		body = new Body(copy.body->shape, copy.body->position);
	}
	~RigidBodyComponent() {
		delete body;
	}
	void Init() {
		rigid_body.Init();
		//LOG("Calculated terminal velocity for " << entity.getID() << " : " << rigidBody.terminalVelocity);
	}
};