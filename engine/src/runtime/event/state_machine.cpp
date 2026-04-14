#include "runtime/event/state_machine.h"

#include <functional>
#include <vector>

#include "runtime/ecs/entity.h"

#include "runtime/scripting/script.h"
#include "runtime/scripting/scripts.h"

namespace ptgn {

namespace impl {

void StateMachine::OnEvent(Entity entity, Event dispatcher) {
	for (const auto& transition : transitions) {
		if (transition.from_state != current_state) {
			continue;
		}

		if (!dispatcher.IsType(transition.event_id)) {
			continue;
		}

		current_state = transition.to_state;

		if (transition.callback) {
			std::invoke(transition.callback, entity);
		}

		if (dispatcher.IsHandled()) {
			return;
		}
	}
}

void StateMachineScript::OnEvent(Event dispatcher) {
	auto* state_machines = entity.TryGet<StateMachines>();
	if (!state_machines) {
		return;
	}

	for (auto& state_machine : state_machines->machines) {
		state_machine.OnEvent(entity, dispatcher);

		if (dispatcher.IsHandled()) {
			return;
		}
	}
}

} // namespace impl

StateMachineBuilder::StateMachineBuilder(impl::StateMachine* state_machine) :
	state_machine_{ state_machine } {}

StateMachineBuilder::TransitionBuilder::TransitionBuilder(
	StateMachineBuilder& parent, impl::StateMachine::Transition& transition
) :
	parent_{ parent }, transition_{ transition } {}

StateMachineBuilder AddStateMachine(Entity e) {
	auto& state_machines{ e.TryAdd<impl::StateMachines>() };

	if (!e.Has<impl::StateMachineScript>()) {
		AddScript<impl::StateMachineScript>(e);
	}

	state_machines.machines.emplace_back();

	return StateMachineBuilder{ &state_machines.machines.back() };
}

} // namespace ptgn