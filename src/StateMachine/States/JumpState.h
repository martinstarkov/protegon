#pragma once

#include "State.h"

class JumpState : public State<JumpState> {
public:
	virtual void update() override final {
		MotionComponent* motion = entity.getComponent<MotionComponent>();
		if (motion->acceleration.y >= 0.0) {
			parentStateMachine->setCurrentState("grounded");
		}
	}
};