#pragma once

#include <engine/Include.h>

struct RandomizeColorEvent {
	static void Invoke(ecs::Entity& invoker) {
		if (invoker.HasComponent<InfluenceComponent>()) {
			auto& manager = invoker.GetComponent<InfluenceComponent>().manager;
			/*auto color_entities = manager.GetComponentTuple<RenderComponent>();
			for (auto [entity2, render_component2] : color_entities) {
				render_component2.original_color = engine::Color::RandomSolid();
			}*/
			auto camera_entities = manager.GetComponentTuple<CameraComponent>();
			for (auto [entity, camera] : camera_entities) {
				camera.primary = false;
			}
			auto entities = manager.GetComponentTuple<RenderComponent>();
			auto count = entities.size();
			auto random_index = engine::math::Random<std::size_t>(0, count - 1);
			for (std::size_t i = 0; i < count; ++i) {
				if (i == random_index) {
					auto [entity, render] = entities[i];
					entity.AddComponent<CameraComponent>(engine::Camera{}, true);
				}
			}
		}
	}
};