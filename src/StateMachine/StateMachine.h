#pragma once

#include <assert.h>

#include "BaseStateMachine.h"
#include "States.h"

template <typename StateMachineType>
class StateMachine : public BaseStateMachine {
public:
	StateMachine() : _currentState(nullptr), _previousStateID(0) {
		_id = static_cast<StateMachineID>(typeid(StateMachineType).hash_code());
	}
	virtual void update() override {
		assert(_currentState && "Undefined starting state");
		_previousStateID = _currentState->getStateID();
		_currentState->update();
	}
	virtual StateMachineID getStateMachineID() override final { return _id; }
	virtual BaseState* getCurrentState() override final { return _currentState.get(); }
	virtual StateID getCurrentStateID() override final { return _currentState->getStateID(); }
	virtual bool stateChangeOccured() override final {
		return _previousStateID != _currentState->getStateID();
	}
	virtual bool isState(StateID state) override final {
		return state == _currentState->getStateID();
	}
	virtual void initState(std::unique_ptr<BaseState> state) override final {
		_currentState = std::move(state);
		_currentState->setParentStateMachine(this);
		_currentState->onEntry();
	}
	virtual void setCurrentState(std::unique_ptr<BaseState> state) override final {
		assert(_currentState && "Undefined starting state");
		if (!isState(state->getStateID())) { // state change
			_previousStateID = _currentState->getStateID();
			_currentState->onExit();
			initState(std::move(state));
		} else {
			state.reset();
		}
	}
private:
	std::unique_ptr<BaseState> _currentState;
	StateID _previousStateID;
	StateMachineID _id;
};

