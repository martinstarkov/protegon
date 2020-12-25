#pragma once

#include <engine/Include.h>

#include "components/TowerComponent.h"

class StandbyState : public engine::State {
	virtual void OnEntry() override final {
		if (parent_entity.HasComponent<RenderComponent>()) {
			auto& render = parent_entity.GetComponent<RenderComponent>();
			render.color = engine::ORANGE;
		}
	}
	virtual void Update() override final {
		assert(parent_entity.HasComponent<TowerComponent>() && "Cannot update standby state without TowerComponent");
		assert(parent_entity.HasComponent<TransformComponent>() && "Cannot update standby state without TransformComponent");
		assert(parent_entity.HasComponent<CollisionComponent>() && "Cannot update standby state without CollisionComponent");
		auto& tower_collider = parent_entity.GetComponent<CollisionComponent>();
		auto& tower = parent_entity.GetComponent<TowerComponent>();
		auto& tower_transform = parent_entity.GetComponent<TransformComponent>();
		auto manager = parent_entity.GetManager();
		assert(manager != nullptr && "Firing entity state parent manager does not exist");
		auto players = manager->GetComponentTuple<TransformComponent, PlayerController, CollisionComponent>();
		ecs::Entity closest_player = ecs::null;
		auto closest_distance = tower.range;
		for (auto [entity, transform, player, collider] : players) {
			double distance = (tower_transform.position + tower_collider.collider.size / 2.0 - (transform.position + collider.collider.size / 2.0)).Magnitude();
			if (distance < closest_distance) {
				closest_player = entity;
				closest_distance = distance;
			}
		}
		if (tower.projectiles > 0) {
			if (closest_player.IsAlive()) {
				if (parent_entity.HasComponent<RenderComponent>()) {
					auto& render = parent_entity.GetComponent<RenderComponent>();
					render.color = engine::GREEN;
				}
				DebugDisplay::circles().emplace_back(tower_transform.position + tower_collider.collider.size / 2.0, tower.range, engine::GREEN);
				if (tower.firing_counter > tower.firing_delay) {
					tower.target = closest_player;
					tower.firing_counter = 0;
					parent_state_machine->SetState("firing");
				} else {
					// Tower in process of firing (waiting on delay).
					++tower.firing_counter;
				}
			} else {
				if (parent_entity.HasComponent<RenderComponent>()) {
					auto& render = parent_entity.GetComponent<RenderComponent>();
					render.color = engine::ORANGE;
				}
				DebugDisplay::circles().emplace_back(tower_transform.position + tower_collider.collider.size / 2.0, tower.range, engine::ORANGE);
				//LOG("Player out of range");
			}
		} else {
			//LOG("Tower ran out of projectiles");
			parent_state_machine->SetState("disabled");
		}
	}
};