#pragma once

#include "Component.h"

#include "../../StateMachine/StateMachines.h"

using RawStateMachineMap = std::map<StateMachineName, BaseStateMachine*>;

struct StateMachineComponent : public Component<StateMachineComponent> {
	StateMachineMap stateMachines;
	// TODO: Fix how the StateMachineMap is initialized and remove the need for a separate setNames function
	StateMachineComponent(RawStateMachineMap&& stateMachines) {
		for (auto& pair : stateMachines) {
			pair.second->setName(pair.first);
			this->stateMachines.emplace(pair.first, std::move(pair.second));
		}
	}
	virtual void init() override final {
		for (auto& pair : stateMachines) {
			pair.second->init(entity);
		}
	}
};
// TODO: Pointer serialization / Don't use pointer container for initialization, i.e. allocate stateMachines inside constructor
//inline void to_json(nlohmann::json& j, const StateMachineComponent& o) {
//	json map;
//	for (auto& pair : o.stateMachines) {
//		map[pair.first] = pair.second.get();
//	}
//	j["stateMachines"] = map;
//}
//
//inline void from_json(const nlohmann::json& j, StateMachineComponent& o) {
//	o = StateMachineComponent(
//		static_cast<RawStateMachineMap>(j.at("start").get<RawStateMachineMap>())
//	);
//}