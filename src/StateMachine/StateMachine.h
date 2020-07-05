#pragma once

#include <assert.h>

#include "BaseStateMachine.h"
#include "States.h"

template <typename StateMachineType>
class StateMachine : public BaseStateMachine {
public:
	StateMachine() : _currentState(nullptr) {
		_id = static_cast<StateMachineID>(typeid(StateMachineType).hash_code());
	}
	virtual void update() override {
		assert(_currentState != nullptr && "Undefined starting state");
		_currentState->update();
	}
	virtual StateMachineID getStateMachineID() override final { return _id; }
	virtual BaseState* getCurrentState() override final { return _currentState; }
	// TODO: Make this a template function which takes a StateType in <> and a parameter pack as argument which it passes to the constructor of that StateType, this way we can avoid head allocating if alreaedy in that state
	virtual void setCurrentState(BaseState* newState) override final {
		StateID currentID = 0; // potential to fail later if a hash_code of 0 is created for a state name
		if (_currentState) { // initial state will ignore any exit call on previous state
			currentID = _currentState->getStateID();
			_currentState->onExit();
		}
		if (newState->getStateID() != currentID) { // state must change
			newState->setParentStateMachine(this);
			delete _currentState;
			_currentState = newState;
			_currentState->onEnter();
		} else {
			delete newState; // prevent memory leak with this temporary hacky solution (move to templates later)
		}
	}
private:
	BaseState* _currentState;
	StateMachineID _id;
};

