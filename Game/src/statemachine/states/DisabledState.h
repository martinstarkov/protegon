#pragma once

#include <engine/Include.h>

#include "components/TowerComponent.h"

class DisabledState : public engine::State {
	virtual void OnEntry() override final {
		if (parent_entity.HasComponent<RenderComponent>()) {
			auto& render = parent_entity.GetComponent<RenderComponent>();
			render.color = engine::BLACK;
		}
	}
	virtual void Update() override final {
		assert(parent_entity.HasComponent<TowerComponent>() && "Cannot update disabled state without TowerComponent");
		assert(parent_entity.HasComponent<TransformComponent>() && "Cannot update disabled state without TransformComponent");
		assert(parent_entity.HasComponent<CollisionComponent>() && "Cannot update standby state without CollisionComponent");
		auto& tower_transform = parent_entity.GetComponent<TransformComponent>();
		auto& tower = parent_entity.GetComponent<TowerComponent>();
		auto& tower_collider = parent_entity.GetComponent<CollisionComponent>();
		DebugDisplay::circles().emplace_back(tower_transform.position + tower_collider.collider.size / 2.0, tower.range, engine::BLACK);
		if (tower.projectiles > 0) {
			parent_state_machine->SetState("standby");
			return;
		} else {
			//LOG("Out of projectiles");
		}
	}
};