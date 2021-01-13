#pragma once

#include <engine/Include.h>

class WorldRenderSystem : public ecs::System<RenderComponent, TransformComponent, CollisionComponent> {
public:
	WorldRenderSystem() = default;
	WorldRenderSystem(engine::Scene* scene) : scene{ scene } {}
	virtual void Update() override final {
		for (auto& [entity, render, transform, collider] : entities) {
			if (!entity.HasComponent<PlayerController>()) {
				engine::TextureManager::DrawRectangle(
					Ceil(transform.position),
					Ceil(collider.collider.size),
					render.color);
			}
		}

		for (auto [aabb, color] : DebugDisplay::rectangles()) {
			engine::TextureManager::DrawRectangle(
				Ceil(aabb.position),
				Ceil(aabb.size),
				color);
		}
		DebugDisplay::rectangles().clear();
		for (auto [origin, destination, color] : DebugDisplay::lines()) {
			engine::TextureManager::DrawLine(
				Ceil(origin),
				Ceil(destination),
				color);
		}
		DebugDisplay::lines().clear();
		for (auto [center, radius, color] : DebugDisplay::circles()) {
			engine::TextureManager::DrawCircle(
				Ceil(center),
				static_cast<int>(radius),
				color);
		}
		DebugDisplay::circles().clear();
	}
private:
	engine::Scene* scene = nullptr;
};