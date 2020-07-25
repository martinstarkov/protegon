#pragma once

#include "System.h"

#include "../../StateMachine/StateMachines.h"

class StateMachineSystem : public System<StateMachineComponent> {
public:
	virtual void update() override final {
		for (auto& id : entities) {
			Entity e = Entity(id, manager);
			StateMachineComponent* sm = e.getComponent<StateMachineComponent>();
			for (const auto& pair : sm->stateMachines) {
				pair.second->update();
			}
		}
	}
};