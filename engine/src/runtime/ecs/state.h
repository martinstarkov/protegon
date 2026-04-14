#pragma once

#include <vector>

#include "core/event/event.h"
#include "core/util/state_machine.h"
#include "runtime/ecs/entity.h"

namespace ptgn {
	
namespace impl {

template <std::copy_constructible TPayload>
struct StateMachines {
	std::vector<StateMachine<TPayload>> machines;
};

class StateMachineScript : public Script {
public:
	void OnEvent(Event event) override {
		auto state_machines{ entity.TryGet<StateMachines<Entity>>() };
		if (!state_machines) {
			return;
		}

		for (auto& state_machine : state_machines->machines) {
			state_machine.OnEvent(event, entity);

			if (event.IsHandled()) {
				return;
			}
		}
	}
};

} // namespace impl

[[nodiscard]] StateMachineBuilder<Entity> AddStateMachine(Entity entity) {
	auto& state_machines{ entity.TryAdd<impl::StateMachines<Entity>>() };

	if (!entity.Has<impl::StateMachineScript>()) {
		AddScript<impl::StateMachineScript>(entity);
	}

	state_machines.machines.emplace_back();

	return StateMachineBuilder<Entity>{ &state_machines.machines.back() };
}

} // namespace ptgn