#include "IdleState.h"

void IdleState::onEnter() {
}

void IdleState::onExit() {
	_sm->setCurrentState(new WalkState(), _sm);
}

void IdleState::update() {
}
