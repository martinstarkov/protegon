#pragma once

#include "State.h"

class GroundedState : public State<GroundedState> {
	virtual void update() override final {
		if (entity.HasComponent<RigidBodyComponent>()) {
			auto& rb = entity.GetComponent<RigidBodyComponent>();
			auto& rigidBody = rb.rigidBody;
			if (rigidBody.acceleration.y < 0.0) { // upward
				parentStateMachine->setCurrentState("jumped");
			}
		} else {
			assert(false && "Cannot update given state without RigidBodyComponent");
		}
	}
};