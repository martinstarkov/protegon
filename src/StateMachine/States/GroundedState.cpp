#include "GroundedState.h"

#include "StateCommon.h"

void GroundedState::update() {
	MotionComponent* motion = entity.getComponent<MotionComponent>();
	if (motion->acceleration.y < 0.0) {
		parentStateMachine->setCurrentState("jumped");
	}
}
