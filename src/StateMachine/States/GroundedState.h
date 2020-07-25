#pragma once

#include "State.h"

class GroundedState : public State<GroundedState> {
	virtual void update() override final {
		MotionComponent* motion = entity.getComponent<MotionComponent>();
		if (motion->acceleration.y < 0.0) {
			parentStateMachine->setCurrentState("jumped");
		}
	}
};