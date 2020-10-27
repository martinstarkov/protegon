#pragma once

#include "System.h"

#include "renderer/TextureManager.h"

class RenderSystem : public ecs::System<RenderComponent, TransformComponent> {
public:
	virtual void Update() override final {
		for (auto& [entity, render_component, transform] : entities) {
			if (entity.HasComponent<SpriteComponent>()) {
				auto& sprite = entity.GetComponent<SpriteComponent>();
				auto flip = Flip::NONE;
				if (entity.HasComponent<DirectionComponent>()) {
					auto& direction = entity.GetComponent<DirectionComponent>().direction;
					if (direction == Direction::LEFT) {
						flip = Flip::HORIZONTAL;
					}
				}
				if (entity.HasComponent<CollisionComponent>()) {
					auto& cc = entity.GetComponent<CollisionComponent>();
					//engine::TextureManager::DrawRectangle(sprite.path, sprite.source.position, sprite.source.size, transform.position, cc.collider.size, flip, 0);
					// Draw only collider boxes (without sprite) for debug purposes.
					engine::TextureManager::DrawRectangle(transform.position, cc.collider.size, render_component.color);
				} else {
					engine::TextureManager::DrawRectangle(sprite.path, sprite.source.position, sprite.source.size, transform.position, sprite.source.size, flip, 0);
				}
			} else {
				if (entity.HasComponent<CollisionComponent>()) {
					auto& cc = entity.GetComponent<CollisionComponent>();
					engine::TextureManager::DrawRectangle(transform.position, cc.collider.size, render_component.color);
				}
			}
		}
		for (auto rectangle : DebugDisplay::rectangles()) {
			engine::TextureManager::DrawRectangle(rectangle.first.position, rectangle.first.size, rectangle.second);
		}
		DebugDisplay::rectangles().clear();
	}
};