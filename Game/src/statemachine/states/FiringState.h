#pragma once

#include <engine/Include.h>

#include "components/TowerComponent.h"

#include "factory/CreateBullet.h"

class FiringState : public engine::State {
	virtual void OnEntry() override final {
		if (parent_entity.HasComponent<RenderComponent>()) {
			auto& render = parent_entity.GetComponent<RenderComponent>();
			render.color = engine::BLACK;
		}
		assert(parent_entity.HasComponent<TowerComponent>() && "Cannot update firing state without TowerComponent");
		assert(parent_entity.HasComponent<TransformComponent>() && "Cannot update firing state without TransformComponent");
		auto& tower = parent_entity.GetComponent<TowerComponent>();
		auto& tower_transform = parent_entity.GetComponent<TransformComponent>();
		auto manager = parent_entity.GetManager();
		assert(manager != nullptr && "Firing entity state parent manager does not exist");
		assert(tower.target.HasComponent<TransformComponent>() && "Tower target must have TransformComponent");
		CreateBullet(tower_transform.position, tower.target.GetComponent<TransformComponent>().position, *manager);
		--tower.projectiles;
		assert(tower.projectiles >= 0 && "Tower must have integer number of projectiles");
		parent_state_machine->SetState("standby");
	}
	virtual void Update() override final {
		assert(parent_entity.HasComponent<TowerComponent>() && "Cannot update firing state without TowerComponent");
		assert(parent_entity.HasComponent<TransformComponent>() && "Cannot update firing state without TransformComponent");
		auto& tower = parent_entity.GetComponent<TowerComponent>();
		auto& transform = parent_entity.GetComponent<TransformComponent>();
		DebugDisplay::circles().emplace_back(transform.position, tower.range, engine::BLACK);
	}
};