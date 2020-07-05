#pragma once

#include "Component.h"

#include "../../StateMachine/StateMachines.h"

struct StateMachineComponent : public Component<StateMachineComponent> {
	// TODO: Find out how to pass parameter pack of BaseStateMachine pointers to the component (because there can only exist one component, we store them all in a vector)
	StateMachineComponent(BaseStateMachine* one, BaseStateMachine* two) {
		_stateMachines.emplace_back(one);
		//_stateMachines.emplace_back(two);
	}
	std::vector<BaseStateMachine*> _stateMachines;
};