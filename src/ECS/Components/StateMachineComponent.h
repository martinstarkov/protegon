//#pragma once
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
//
//// TODO: Pointer serialization / Don't use pointer container for initialization, i.e. allocate stateMachines inside constructor
//inline void to_json(nlohmann::json& j, const StateMachineComponent& o) {
////	/*
////	json map;
////	for (auto& pair : o.stateMachines) {
////		map[pair.first] = pair.second.get();
////	}
////	j["stateMachines"] = map;
////	*/
//}
//
//inline void from_json(const nlohmann::json& j, StateMachineComponent& o) {
////	o = StateMachineComponent();
////	/*if (j.find("stateMachines") != j.end()) {
////		o.stateMachines = j.at("stateMachines").get<StateMachineMap>();
////	}*/
//}