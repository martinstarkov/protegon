#include "StateMachine.h"

StateMachine::StateMachine(StateName initialState) : _currentState(initialState), _previousState(initialState), _name(UNKNOWN_STATE_MACHINE) {}

void StateMachine::init(Entity handle) {
	for (const auto& pair : states) {
		pair.second->setName(pair.first);
		pair.second->setParentStateMachine(this);
		pair.second->setHandle(handle);
	}
}

void StateMachine::update() {
	assert(states.find(_currentState) != states.end() && "Undefined starting state");
	states[_currentState]->update();
}

StateMachineName StateMachine::getName() {
	return _name;
}

void StateMachine::setName(StateMachineName name) {
	_name = name;
}

BaseState* StateMachine::getCurrentState() {
	return states[_currentState].get();
}

void StateMachine::setCurrentState(StateName state) {
	if (state != _currentState) {
		_previousState = _currentState;
		_currentState = state;
		states[_previousState]->onExit();
		states[_currentState]->onEntry();
	}
}

bool StateMachine::inState(StateName name) {
	return _currentState == name;
}