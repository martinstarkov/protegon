#include "IdleState.h"

#include "StateCommon.h"

void IdleState::onEntry() {
	AnimationComponent* animation = entity.getComponent<AnimationComponent>();
	if (animation) {
		animation->name = getName();
		animation->counter = -1;
	}
}

void IdleState::update() {
	MotionComponent* motion = entity.getComponent<MotionComponent>();
	if (abs(motion->velocity) >= IDLE_START_VELOCITY) {
		parentStateMachine->setCurrentState("walk");
	}
}