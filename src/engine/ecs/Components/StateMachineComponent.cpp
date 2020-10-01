//#include "StateMachineComponent.h"
//
//#include <StateMachine/BaseStateMachine.h>
//
//StateMachineComponent::StateMachineComponent(ecs::Entity entity, RawStateMachineMap&& stateMachines) {
//	// CHECK FOR MEMORY LEAKS
//	for (auto& pair : stateMachines) {
//		pair.second->setName(pair.first);
//		this->stateMachines.emplace(pair.first, std::move(pair.second));
//	}
//	for (auto& pair : stateMachines) {
//		pair.second->init(entity);
//	}
//}
//StateMachineComponent::StateMachineComponent(const StateMachineComponent& copy) {
//	for (auto& pair : copy.stateMachines) {
//		stateMachines.emplace(pair.first, pair.second->uniqueClone());
//	}
//}