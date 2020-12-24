#pragma once

#include <engine/Include.h>

#include "statemachine/FiringStateMachine.h"

ecs::Entity CreateSentry(V2_double position, ecs::Manager& manager) {
	auto entity = manager.CreateEntity();
	auto scale = V2_double{ 1, 1 };
	entity.AddComponent<TransformComponent>(position);
	auto& sm = entity.AddComponent<StateMachineComponent>();
	sm.AddStateMachine<FiringStateMachine>("firing_state_machine", entity);
	V2_int collider_size = V2_double{ 32, 32 } * scale;
	entity.AddComponent<CollisionComponent>(position, collider_size);
	entity.AddComponent<TowerComponent>(3000, 500, 30);
	entity.AddComponent<RenderComponent>();
	return entity;
}