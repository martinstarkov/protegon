#pragma once

#include "System.h"

#include "../../StateMachine/StateMachines.h"

class StateMachineSystem : public System<StateMachineComponent> {
public:
	virtual void update() override {
		for (auto& entityID : entities) {
			Entity& e = getEntity(entityID);
			StateMachineComponent* sm = e.getComponent<StateMachineComponent>();
			for (const auto& pair : sm->stateMachines) {
				pair.second->update();
				if (pair.first == "walkStateMachine") {
					MotionComponent* motion = e.getComponent<MotionComponent>();
					//LOG_(pair.second->getName() << ": " << pair.second->getCurrentState()->getName() << ", velocity: " << motion->velocity);
				} else if (pair.first == "jumpStateMachine") {
					MotionComponent* motion = e.getComponent<MotionComponent>();
					//LOG(" | " << pair.second->getName() << ": " << pair.second->getCurrentState()->getName() << ", acceleration: " << motion->acceleration);
				}
			}
		}
	}
};