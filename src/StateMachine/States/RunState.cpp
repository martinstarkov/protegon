#include "RunState.h"

#include "StateCommon.h"

void RunState::onEntry() {
	AnimationComponent* animation = entity.getComponent<AnimationComponent>();
	if (animation) {
		animation->name = getName();
		animation->counter = -1;
	}
}

void RunState::update() {
	MotionComponent* motion = entity.getComponent<MotionComponent>();
	if (!(abs(motion->velocity) > IDLE_START_VELOCITY)) {
		parentStateMachine->setCurrentState("idle");
	} else if (!(abs(motion->velocity) >= motion->terminalVelocity * RUN_START_FRACTION)) {
		parentStateMachine->setCurrentState("walk");
	}
}
