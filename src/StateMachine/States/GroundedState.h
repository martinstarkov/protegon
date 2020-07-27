#pragma once

#include "State.h"

class GroundedState : public State<GroundedState> {
	virtual void update() override final {
		RigidBodyComponent* rb = entity.getComponent<RigidBodyComponent>();
		assert(rb && "Cannot update given state without RigidBodyComponent");
		RigidBody& rigidBody = rb->rigidBody;
		if (rigidBody.acceleration.y < 0.0) { // upward
			parentStateMachine->setCurrentState("jumped");
		}
	}
};