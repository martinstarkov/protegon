//#pragma once

// TODO: Complete rework.


//
//#include "Component.h"
//
//// TODO: Fix how the StateMachineMap is initialized and remove the need for a separate setNames function
//
//#include <map>
//#include <memory>
//
//class BaseStateMachine;
//
//using RawStateMachineMap = std::map<std::string, BaseStateMachine*>;
//using StateMachineMap = std::map<std::string, std::unique_ptr<BaseStateMachine>>;
//
//struct StateMachineComponent {
//	StateMachineMap stateMachines;
//	StateMachineComponent() = default;
//	StateMachineComponent(ecs::Entity entity, RawStateMachineMap&& stateMachines);
//	StateMachineComponent(const StateMachineComponent& copy);
//};