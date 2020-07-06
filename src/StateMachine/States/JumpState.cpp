#include "JumpState.h"
#include "StateCommon.h"

void JumpState::onEnter() {}

void JumpState::onExit() {}

void JumpState::update() {
	static int airTime = 0;
	if (airTime >= 50) {
		_sm->setCurrentState(new GroundedState());
		airTime = 0;
	} else {
		airTime++;
	}
	LOG_(" , Jumping: " << airTime << ", ");
}
