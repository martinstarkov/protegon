#include "StateMachineSystem.h"

#include "SystemCommon.h"

#include "../../StateMachine/StateMachines.h"

void StateMachineSystem::update() {
	for (auto& id : entities) {
		Entity e = Entity(id, manager);
		StateMachineComponent* sm = e.getComponent<StateMachineComponent>();
		for (const auto& pair : sm->stateMachines) {
			pair.second->update();
		}
	}
}
