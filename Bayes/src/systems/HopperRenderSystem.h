#pragma once

#include <engine/Include.h>

class HopperRenderSystem : public ecs::System<PlayerController, RenderComponent, TransformComponent, CollisionComponent> {
public:
	HopperRenderSystem() = default;
	HopperRenderSystem(engine::Scene* scene) : scene{ scene } {}
	virtual void Update() override final {
		for (auto& [player, entity, render, transform, collision] : entities) {
			engine::TextureManager::DrawRectangle(transform.position, collision.collider.size, transform.rotation, transform.center_of_rotation, render.color);
		}
	}
private:
	engine::Scene* scene = nullptr;
};