#include "WalkState.h"
#include "StateCommon.h"

void WalkState::onEnter() {
	_walkTime = 0;
}

void WalkState::onExit() {
}

void WalkState::update() {
	if (_walkTime >= RUN_THRESHOLD) {
		_sm->setCurrentState(new RunState());
	}
	_walkTime++;
	LOG("Walking: " << _walkTime);
}
