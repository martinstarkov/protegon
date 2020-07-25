#pragma once

#include "State.h"

class IdleState : public State<IdleState> {
	virtual void onEntry() override final {
		AnimationComponent* animation = entity.getComponent<AnimationComponent>();
		if (animation) {
			animation->name = getName();
			animation->counter = -1;
		}
	}
	virtual void update() override final {
		MotionComponent* motion = entity.getComponent<MotionComponent>();
		if (abs(motion->velocity) >= IDLE_START_VELOCITY) {
			parentStateMachine->setCurrentState("walk");
		}
	}
};