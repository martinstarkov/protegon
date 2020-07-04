#pragma once

#include "BaseStateMachine.h"
#include "States.h"

class BaseState;

template <typename StateMachineType>
class StateMachine : public BaseStateMachine {
public:
	StateMachine() : _currentState(nullptr) { _id = static_cast<StateMachineID>(typeid(StateMachineType).hash_code()); }
	virtual void update() override { _currentState->update(); }
	virtual StateMachineID getStateMachineID() override final { return _id; }
	virtual BaseState* getCurrentState() override final { return _currentState; }
	virtual void setCurrentState(BaseState* newState, BaseStateMachine* stateMachine = this) override final {
		newState->setParentStateMachine(stateMachine);
		_currentState->onExit();
		delete _currentState;
		_currentState = newState;
		_currentState->onEntry();
	}
private:
	BaseState* _currentState;
	StateMachineID _id;
};

