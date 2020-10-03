#pragma once

#include "State.h"

class WalkState : public State<WalkState> {
public:
	virtual void onEntry() override final {
		if (entity.HasComponent<AnimationComponent>()) {
			auto& animation = entity.GetComponent<AnimationComponent>();
			animation.name = getName();
			animation.counter = -1;
		}
	}
	virtual void update() override final {
		if (entity.HasComponent<RigidBodyComponent>()) {
			auto& rb = entity.GetComponent<RigidBodyComponent>();
			auto& rigidBody = rb.rigidBody;
			if (abs(rigidBody.velocity) >= rigidBody.terminalVelocity * RUN_START_FRACTION) {
				//parentStateMachine->setCurrentState("run");
			} else if (!(abs(rigidBody.velocity) > IDLE_START_VELOCITY)) {
				parentStateMachine->setCurrentState("idle");
			}
		} else {
			assert(false && "Cannot update given state without RigidBodyComponent");
		}
	}
};

