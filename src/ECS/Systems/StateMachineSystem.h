#pragma once

#include "System.h"

#include "../../StateMachine/StateMachines.h"

class StateMachineSystem : public System<StateMachineComponent> {
public:
	virtual void update() override {
		for (auto& entityID : _entities) {
			Entity& e = getEntity(entityID);
			StateMachineComponent* _sm = e.getComponent<StateMachineComponent>();
			for (BaseStateMachine* _stateMachine : _sm->_stateMachines) {
				_stateMachine->update();
			}
		}
	}
};