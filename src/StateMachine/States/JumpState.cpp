#include "JumpState.h"

#include "StateCommon.h"

void JumpState::update() {
	MotionComponent* motion = entity.getComponent<MotionComponent>();
	if (motion->acceleration.y >= 0.0) {
		parentStateMachine->setCurrentState("grounded");
	}
}
