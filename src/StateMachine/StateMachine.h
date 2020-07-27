#pragma once

#include "BaseStateMachine.h"
#include "States/BaseState.h"

template <typename T>
class StateMachine : public BaseStateMachine {
public:
	StateMachine() = default;
	StateMachine(StateName initialState) : _currentState(initialState), _previousState(initialState), _name(UNKNOWN_STATE_MACHINE) {}
	StateMachine(const StateMachine& copy) {
		_name = copy._name;
		_currentState = copy._currentState;
		_previousState = copy._previousState;
		for (const auto& pair : copy.states) {
			states.emplace(pair.first, pair.second->uniqueClone());
		}
	}
	virtual ~StateMachine() = default;
	virtual BaseStateMachine* clone() const override final {
		return new T(static_cast<const T&>(*this));
	}
	virtual std::unique_ptr<BaseStateMachine> uniqueClone() const override final {
		return std::make_unique<T>(static_cast<const T&>(*this));
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
	virtual void init(Entity handle) override final {
		for (const auto& pair : states) {
			pair.second->setName(pair.first);
			pair.second->setParentStateMachine(this);
			pair.second->setHandle(handle);
		}
		states[_currentState]->onEntry();
	}
	StateMap states;
private:
	StateMachineName _name;
	StateName _currentState;
	StateName _previousState;
};