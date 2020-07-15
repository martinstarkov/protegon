#pragma once

#include "System.h"

#include "../../StateMachine/StateMachines.h"

class StateMachineSystem : public System<StateMachineComponent> {
public:
	virtual void update() override {
		for (auto& entityID : entities) {
			Entity& e = getEntity(entityID);
			StateMachineComponent* sm = e.getComponent<StateMachineComponent>();
			for (BaseStateMachine* stateMachine : sm->stateMachines) {
				stateMachine->update();
			}
		}
	}
};