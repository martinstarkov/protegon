#pragma once

#include <map>

#include "Types.h"

#include "States/BaseState.h"

class Entity;

class StateMachine {
public:
	StateMachine(Entity& entity);
public:
	struct StateFactory {
		template <typename ...Ts> static void call(StateMachine& stateMachine, Ts&&... args) {
			swallow((stateMachine.createState(args), 0)...);
		}
	};
public:
	template<class TFunctor, typename... Ts> auto create(Ts&&... args) {
		return TFunctor::call(*this, std::forward<Ts>(args)...);
	}
	template <class TState> void createState(TState& state) {
		StateID id = static_cast<StateID>(typeid(TState).hash_code());
		assert(_states.find(id) == _states.end());
		std::unique_ptr<TState> uPtr = std::make_unique<TState>(std::move(state));
		_states.emplace(id, std::move(uPtr));
	}
	template <typename TState> void setState() {
		StateID newState = static_cast<StateID>(typeid(TState).hash_code());
		auto iterator = _states.find(newState);
		assert(iterator != _states.end() && "Unable to set state as it has not been created for the entity");
		if (_currentState != newState) {
			if (_currentState && _currentState != _previousState) {
				_states[_currentState]->exit(_entity);
				_previousState = _currentState;
				LOG_("Setting previous state to: " << _previousState << ",");
			}
			_states[newState]->enter(_entity);
			_currentState = newState;
			LOG("new state to: " << _currentState);
		}
	}
	template <typename TState> TState& getState() {
		StateID stateID = static_cast<StateID>(typeid(TState).hash_code());
		assert(_states.find(stateID) != _states.end());
		return static_cast<TState&>(*_states[stateID]);
	}
private:
	std::map<StateID, std::unique_ptr<BaseState>> _states;
	Entity& _entity;
	StateID _currentState;
	StateID _previousState;
};