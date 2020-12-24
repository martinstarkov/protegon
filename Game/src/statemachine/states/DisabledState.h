#pragma once

#include <engine/Include.h>

#include "components/TowerComponent.h"

class DisabledState : public engine::State {
	virtual void OnEntry() override final {
		if (parent_entity.HasComponent<RenderComponent>()) {
			auto& render = parent_entity.GetComponent<RenderComponent>();
			render.color = engine::RED;
		}
	}
	virtual void Update() override final {
		assert(parent_entity.HasComponent<TowerComponent>() && "Cannot update disabled state without TowerComponent");
		assert(parent_entity.HasComponent<TransformComponent>() && "Cannot update disabled state without TransformComponent");
		auto& tower_transform = parent_entity.GetComponent<TransformComponent>();
		auto& tower = parent_entity.GetComponent<TowerComponent>();
		DebugDisplay::circles().emplace_back(tower_transform.position, tower.range, engine::RED);
		if (tower.projectiles > 0) {
			parent_state_machine->SetState("standby");
		} else {
			LOG("Out of projectiles");
		}
	}
};