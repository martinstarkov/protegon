#pragma once

#include "State.h"

class JumpState : public State<JumpState> {
public:
	virtual void update() override final {
		RigidBodyComponent* rb = entity.getComponent<RigidBodyComponent>();
		assert(rb && "Cannot update given state without RigidBodyComponent");
		RigidBody& rigidBody = rb->rigidBody;
		// TODO: Change to check for collision instead of acceleration
		if (rigidBody.acceleration.y >= 0.0) {
			parentStateMachine->setCurrentState("grounded");
		}
	}
};