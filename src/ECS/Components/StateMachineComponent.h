#pragma once

#include "Component.h"

#include "../../StateMachine/StateMachines.h"

struct StateMachineComponent : public Component<StateMachineComponent> {
	StateMachineMap stateMachines;
	// TODO: Fix how the StateMachineMap is initialized and remove the need for a separate setNames function
	StateMachineComponent(std::map<StateMachineName, BaseStateMachine*>&& stateMachines) {
		for (auto& pair : stateMachines) {
			pair.second->setName(pair.first);
			this->stateMachines.emplace(pair.first, std::move(pair.second));
		}
	}
};