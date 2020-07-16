#pragma once

#include "Component.h"

#include "../../StateMachine/StateMachines.h"

struct StateMachineComponent : public Component<StateMachineComponent> {
	StateMachineMap stateMachines;
	// TODO: Fix how the StateMachineMap is initialized and remove the need for a separate setNames function
	StateMachineComponent() {
	}
	void setNames() {
		for (const auto& pair : stateMachines) {
			pair.second->setName(pair.first);
		}
	}
};