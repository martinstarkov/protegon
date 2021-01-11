#pragma once

#include "System.h"

#include "renderer/TextureManager.h"

#include "core/Scene.h"

class HitboxRenderSystem : public ecs::System<RenderComponent, TransformComponent> {
public:
	HitboxRenderSystem() = default;
	HitboxRenderSystem(engine::Scene* scene) : scene{ scene } {}
	virtual void Update() override final {
		for (auto& [entity, render, transform] : entities) {
			V2_double position = transform.position;
			V2_double size;
			if (entity.HasComponent<SizeComponent>()) {
				size = entity.GetComponent<SizeComponent>().size;
			} else if (entity.HasComponent<CollisionComponent>()) {
				size = entity.GetComponent<CollisionComponent>().collider.size;
			}
			// TODO: Temporarily exclude player hitboxes.
			if (scene && !entity.HasComponent<PlayerController>()) {
				auto camera = scene->GetCamera();
				if (camera) {
					size *= camera->scale;
					position -= camera->offset;
					position *= camera->scale;
				}
				engine::TextureManager::DrawSolidRectangle(
					position,
					size,
					render.color);
			}
		}
		if (scene) {
			auto camera = scene->GetCamera();
			if (camera) {
				for (auto [aabb, color] : DebugDisplay::rectangles()) {
					engine::TextureManager::DrawRectangle(
						(aabb.position - camera->offset) * camera->scale,
						aabb.size * camera->scale,
						color);
				}
				DebugDisplay::rectangles().clear();
				for (auto [origin, destination, color] : DebugDisplay::lines()) {
					engine::TextureManager::DrawLine(
						(origin - camera->offset) * camera->scale,
						(destination - camera->offset) * camera->scale,
						color);
				}
				DebugDisplay::lines().clear();
				for (auto [center, radius, color] : DebugDisplay::circles()) {
					engine::TextureManager::DrawCircle(
						(center - camera->offset) * camera->scale,
						static_cast<int>(engine::math::FastRound(radius * camera->scale.x)),
						color);
				}
				DebugDisplay::circles().clear();
			}
		}
	}
private:
	engine::Scene* scene = nullptr;
};