#include "StateMachineSystem.h"

#include "../Components/StateMachineComponent.h"
//#include "../Components/MotionComponent.h"

#include "../../StateMachine/StateMachines.h"

void StateMachineSystem::update() {
	for (auto& id : entities) {
		EntityHandle e = EntityHandle(id, manager);
		StateMachineComponent* sm = e.getComponent<StateMachineComponent>();
		for (const auto& pair : sm->stateMachines) {
			pair.second->update();
			if (pair.first == "walkStateMachine") {
				//MotionComponent* motion = e.getComponent<MotionComponent>();
				//LOG_(pair.second->getName() << ": " << pair.second->getCurrentState()->getName() << ", velocity: " << motion->velocity);
			} else if (pair.first == "jumpStateMachine") {
				//MotionComponent* motion = e.getComponent<MotionComponent>();
				//LOG(" | " << pair.second->getName() << ": " << pair.second->getCurrentState()->getName() << ", acceleration: " << motion->acceleration);
			}
		}
	}
}
