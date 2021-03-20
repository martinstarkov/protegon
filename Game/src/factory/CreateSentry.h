#pragma once

#include <engine/Include.h>

#include "statemachine/FiringStateMachine.h"

inline ecs::Entity CreateSentry(const V2_double& position, ecs::Manager& manager) {
	auto entity{ manager.CreateEntity() };
	V2_double scale{ 1.0, 1.0 };
	entity.AddComponent<TransformComponent>(position);
	auto& sm{ entity.AddComponent<StateMachineComponent>() };
	sm.AddStateMachine<FiringStateMachine>("firing_state_machine", entity);
	V2_int collider_size = V2_double{ 32, 32 } * scale;
	auto& collider = entity.AddComponent<CollisionComponent>(position, collider_size);
	collider.ignored_tag_types.push_back(69);
	entity.AddComponent<TowerComponent>(300, 250, 3);
	entity.AddComponent<RenderComponent>();
	return entity;
}