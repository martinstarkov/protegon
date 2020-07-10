#include "JumpState.h"
#include "StateCommon.h"

void JumpState::update() {
	static int airTime = 0;
	if (airTime >= 50) {
		_sm->setCurrentState(std::move(std::make_unique<GroundedState>()));
		airTime = 0;
	} else {
		airTime++;
	}
}
