#pragma once

#include <functional>
#include <vector>

#include "core/event/event.h"
#include "core/util/hash.h"
#include "runtime/ecs/entity.h"
#include "core/event/event_dispatcher.h"
#include "runtime/scripting/script.h"

namespace ptgn {

namespace impl {

struct StateMachine {
	struct Transition {
		std::size_t from_state;
		std::size_t event_id;
		std::size_t to_state;
		std::function<void(Entity)> callback;
	};

	std::size_t current_state{ 0 };
	std::vector<Transition> transitions;

	void OnEvent(Entity entity, EventDispatcher dispatcher);
};

struct StateMachines {
	std::vector<StateMachine> machines;
};

class StateMachineScript : public Script {
public:
	void OnEvent(EventDispatcher dispatcher) override;
};

} // namespace impl

class StateMachineBuilder {
public:
	StateMachineBuilder() = default;

	explicit StateMachineBuilder(impl::StateMachine* state_machine);

	template <typename TState>
	StateMachineBuilder& Initial() {
		state_machine_->current_state = Hash<TState>();
		return *this;
	}

	template <typename FromState, EventType Event, typename ToState>
	auto Transition() {
		impl::StateMachine::Transition transition;

		transition.from_state = Hash<FromState>();
		transition.event_id	  = Event::TypeId();
		transition.to_state	  = Hash<ToState>();

		state_machine_->transitions.push_back(transition);

		return TransitionBuilder{ *this, state_machine_->transitions.back() };
	}

private:
	class TransitionBuilder {
	public:
		TransitionBuilder(StateMachineBuilder& parent, impl::StateMachine::Transition& transition);

		template <typename FromState, EventType Event, typename ToState>
		TransitionBuilder Transition() {
			return parent_.Transition<FromState, Event, ToState>();
		}

		template <typename Fn>
		StateMachineBuilder& Action(Fn fn) {
			transition_.callback = fn;
			return parent_;
		}

	private:
		StateMachineBuilder& parent_;
		impl::StateMachine::Transition& transition_;
	};

	impl::StateMachine* state_machine_{ nullptr };
};

[[nodiscard]] StateMachineBuilder AddStateMachine(Entity entity);

} // namespace ptgn