#pragma once

#include "statemachine/BaseStateMachine.h"
#include "statemachine/StateMachines.h"
#include "utils/math/Hasher.h"

#include <memory> // std::make_unique, std::unique_ptr

struct StateMachineComponent {
	std::unordered_map<std::size_t, std::unique_ptr<engine::BaseStateMachine>> state_machines;
	StateMachineComponent() = default;
	template <typename T>
	void AddStateMachine(const char* name, ecs::Entity parent_entity) {
		auto key = engine::Hasher::HashCString(name);
		state_machines.emplace(key, std::make_unique<T>(parent_entity));
	}
};