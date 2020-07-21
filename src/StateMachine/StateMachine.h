#pragma once

#include <assert.h>

#include "BaseStateMachine.h"
#include "States/BaseState.h"

template <typename StateMachineType>
class StateMachine : public BaseStateMachine {
public:
	StateMachine() : _currentState(UNKNOWN_STATE), _previousState(UNKNOWN_STATE), _name(typeid(StateMachineType).name()) {}
	virtual void init(StateName initialState, EntityHandle handle) override final {
		for (const auto& pair : states) {
			pair.second->setName(pair.first);
			pair.second->setParentStateMachine(this);
			pair.second->setHandle(handle);
		}
		_currentState = initialState;
		_previousState = _currentState;
	}
	virtual void update() override final {
		assert(states.find(_currentState) != states.end() && "Undefined starting state");
		states[_currentState]->update();
	}
	virtual StateMachineName getName() override final {
		return _name;
	}
	virtual void setName(StateMachineName name) override final {
		_name = name;
	}
	virtual BaseState* getCurrentState() override final {
		return states[_currentState].get();
	}
	virtual void setCurrentState(StateName state) override final {
		if (state != _currentState) {
			_previousState = _currentState;
			_currentState = state;
			states[_previousState]->onExit();
			states[_currentState]->onEntry();
		}
	}
	virtual bool inState(StateName name) override final {
		return _currentState == name;
	}
protected:
	StateMap states;
private:
	StateMachineName _name;
	StateName _currentState;
	StateName _previousState;
};