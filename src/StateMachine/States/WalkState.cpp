#include "WalkState.h"
#include "StateCommon.h"

void WalkState::update() {
	static int walkTime = 0;
	if (walkTime >= RUN_THRESHOLD) {
		_sm->setCurrentState(std::move(std::make_unique<RunState>()));
		walkTime = 0;
	} else {
		walkTime++;
	}
}
