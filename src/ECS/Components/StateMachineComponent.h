#pragma once

#include "Component.h"

#include "../../StateMachine/StateMachines.h"

struct StateMachineComponent : public Component<StateMachineComponent> {
	StateMachineMap stateMachines;
	StateMachineComponent() {
	}
	void setNames() {
		for (const auto& pair : stateMachines) {
			pair.second->setName(pair.first);
		}
	}
};