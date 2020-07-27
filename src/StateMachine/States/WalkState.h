#pragma once

#include "State.h"

class WalkState : public State<WalkState> {
public:
	virtual void onEntry() override final {
		AnimationComponent* animation = entity.getComponent<AnimationComponent>();
		if (animation) {
			animation->name = getName();
			animation->counter = -1;
		}
	}
	virtual void update() override final {
		RigidBodyComponent* rb = entity.getComponent<RigidBodyComponent>();
		assert(rb && "Cannot update given state without RigidBodyComponent");
		RigidBody& rigidBody = rb->rigidBody;
		if (abs(rigidBody.velocity) >= rigidBody.terminalVelocity * RUN_START_FRACTION) {
			//parentStateMachine->setCurrentState("run");
		} else if (!(abs(rigidBody.velocity) > IDLE_START_VELOCITY)) {
			parentStateMachine->setCurrentState("idle");
		}
	}
};

