#pragma once

#include "System.h"

#include "renderer/TextureManager.h"

#include "core/Scene.h"

class RenderSystem : public ecs::System<RenderComponent, TransformComponent> {
public:
	RenderSystem() = default;
	RenderSystem(engine::Scene* scene) : scene{ scene } {}
	virtual void Update() override final {
		for (auto& [entity, render, transform] : entities) {
			V2_double position = transform.position;
			V2_double size;
			bool has_sprite = entity.HasComponent<SpriteComponent>();
			if (entity.HasComponent<CollisionComponent>()) {
				size = entity.GetComponent<CollisionComponent>().collider.size;
			}
			if (has_sprite) {
				auto& sprite = entity.GetComponent<SpriteComponent>();
				// Prioritize collider size over sprite size
				if (size.IsZero()) size = sprite.source.size;
				auto flip = Flip::NONE;
				if (entity.HasComponent<DirectionComponent>()) {
					auto& direction = entity.GetComponent<DirectionComponent>().direction;
					if (direction == Direction::LEFT) {
						flip = Flip::HORIZONTAL;
					}
				}
				if (scene) {
					const auto& camera = scene->GetCamera();
					position += camera.offset;
					position *= camera.scale;
					size *= camera.scale;
					engine::TextureManager::DrawRectangle(sprite.path, sprite.source.position, sprite.source.size, position, size, flip, 0);
				}
			} else {
				if (scene) {
					const auto& camera = scene->GetCamera();
					position += camera.offset;
					position *= camera.scale;
					size *= camera.scale;
					if (transform.rotation != 0) {
						engine::TextureManager::DrawRectangle(position, size, transform.rotation, &transform.center_of_rotation, render.color);
					} else {
						engine::TextureManager::DrawRectangle(position, size, render.color);
					}
				}
			}

		}
		for (auto rectangle : DebugDisplay::rectangles()) {
			engine::TextureManager::DrawRectangle(rectangle.first.position, rectangle.first.size, rectangle.second);
		}
		DebugDisplay::rectangles().clear();
	}
private:
	engine::Scene* scene = nullptr;
};