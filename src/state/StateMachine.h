#pragma once

#include <cstdlib> // std::size_t
#include <limits> // std::numeric_limits
#include <stack> // std::stack
#include <unordered_map> // std::unordered_map

#include "state/State.h"
#include "event/Observer.h"

namespace ptgn {

namespace state {

using state = const char*;

class StateMachine : public event::Dispatcher {
public:
	template <typename T>
	void AddState(state state, T&& lambda) {
		event::Listener l = Subscribe(std::move(lambda));
		states.emplace(state, l);
	}
	void PopState() {
		stack.pop();
	}
	void PushState(state state) {
		stack.push(state);
	}
	template <typename T>
	void Notify(state state, T& event) {
		event::Listener l = states.find(state)->second;
		l.Post(event);
		PushState(state);
	}
	state GetCurrentState() const {
		return stack.top();
	}
private:
	std::unordered_map<state, event::Listener> states;
	std::stack<state> stack;
};

/*
using Id = std::size_t;

class StateMachine : public managers::ResourceManager<State> {
public:
	// TODO: Check that TState inherits from State.
	template <typename TState>
	void AddState() {
		auto id = GetStateId<TState>();
		Load(id, new TState{});
	}
	// TODO: Check that enter function takes args using template helpers.
	// TODO: Check that TState inherits from State.
	// TODO: Consider if there is such a thing as passing exist args.
	template <typename TState, typename ...TArgs>
	void SetState(TArgs&&... args) {
		auto id = GetStateId<TState>();
		auto state = static_cast<TState*>(Get(id));
		if (id != previous_state) {
			if (previous_state != std::numeric_limits<Id>::max()) {
				Get(previous_state)->Exit();
			}
			state->Enter(std::forward<TArgs>(args)...);
			previous_state = id;
		}
	}
private:
	// TODO: Check that TState inherits from State.
	template <typename TState>
	static Id GetStateId() {
		static Id id{ StateCount()++ };
		return id;
	}
	static Id& StateCount() {
		static Id id{ 0 };
		return id;
	}
	Id previous_state{ std::numeric_limits<Id>::max() };
};
*/

} // namespace state

} // namespace ptgn