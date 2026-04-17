#pragma once

#include <concepts>
#include <functional>
#include <vector>

#include "core/event/event.h"
#include "core/util/hash.h"

namespace ptgn {

namespace impl {

template <std::copy_constructible TPayload>
struct StateMachine {
	struct Transition {
		std::size_t from_state{ 0 };
		std::size_t event_id{ 0 };
		std::size_t to_state{ 0 };
		std::function<void(TPayload)> callback;
	};

	std::size_t current_state{ 0 };
	std::vector<Transition> transitions;

	void OnEvent(Event event, TPayload payload) {
		for (const auto& transition : transitions) {
			if (transition.from_state != current_state) {
				continue;
			}

			if (!event.IsType(transition.event_id)) {
				continue;
			}

			current_state = transition.to_state;

			if (transition.callback) {
				std::invoke(transition.callback, payload);
			}

			if (event.IsHandled()) {
				return;
			}
		}
	}
};

} // namespace impl

template <std::copy_constructible TPayload>
class StateMachineBuilder {
public:
	StateMachineBuilder() = default;

	explicit StateMachineBuilder(impl::StateMachine<TPayload>* state_machine) :
		state_machine_{ state_machine } {}

	template <typename TState>
	StateMachineBuilder& Initial() {
		state_machine_->current_state = Hash<TState>();
		return *this;
	}

	template <typename FromState, typename TEvent, typename ToState>
	auto Transition() {
		typename impl::StateMachine<TPayload>::Transition transition{};

		transition.from_state = Hash<FromState>();
		transition.event_id	  = Hash<TEvent>();
		transition.to_state	  = Hash<ToState>();

		state_machine_->transitions.push_back(transition);

		return TransitionBuilder{ *this, state_machine_->transitions.back() };
	}

private:
	class TransitionBuilder {
	public:
		TransitionBuilder(
			StateMachineBuilder& parent, impl::StateMachine<TPayload>::Transition& transition
		) :
			parent_{ parent }, transition_{ transition } {}

		template <typename FromState, typename TEvent, typename ToState>
		TransitionBuilder Transition() {
			return parent_.Transition<FromState, TEvent, ToState>();
		}

		template <typename Fn>
		StateMachineBuilder& Action(Fn fn) {
			transition_.callback = fn;
			return parent_;
		}

	private:
		StateMachineBuilder& parent_;
		impl::StateMachine<TPayload>::Transition& transition_;
	};

	impl::StateMachine<TPayload>* state_machine_{ nullptr };
};

} // namespace ptgn