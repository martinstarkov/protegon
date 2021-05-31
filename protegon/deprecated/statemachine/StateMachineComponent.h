#pragma once

#include "statemachine/BaseStateMachine.h"
#include "statemachine/StateMachines.h"
#include "math/Hasher.h"

#include <memory> // std::make_unique, std::unique_ptr
#include <cstdlib> // std::size_t

struct StateMachineComponent {
	std::unordered_map<std::size_t, std::shared_ptr<engine::BaseStateMachine>> state_machines;
	StateMachineComponent() = default;
	template <typename T>
	void AddStateMachine(const char* name, const ecs::Entity& parent_entity) {
		auto key{ engine::Hasher::HashCString(name) };
		state_machines.emplace(key, std::make_shared<T>(parent_entity));
	}
};