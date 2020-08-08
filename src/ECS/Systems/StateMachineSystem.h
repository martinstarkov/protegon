#pragma once

#include "System.h"

#include "../../StateMachine/StateMachines.h"

class StateMachineSystem : public System<StateMachineComponent> {
public:
	virtual void update() override final {
		for (auto& id : entities) {
			auto [stateMachineComponent] = getComponents(id);
			for (const auto& pair : stateMachineComponent.stateMachines) {
				pair.second->update();
			}
		}
	}
};