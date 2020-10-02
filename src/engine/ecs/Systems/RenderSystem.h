#pragma once

#include "System.h"

#include <engine/renderer/TextureManager.h>

class RenderSystem : public ecs::System<RenderComponent, TransformComponent> {
public:
	virtual void Update() override final {
		for (auto& [entity, renderComponent, transform] : entities) {
			if (entity.HasComponent<SpriteComponent>()) {
				auto& sprite = entity.GetComponent<SpriteComponent>();
				auto flip = Flip::NONE;
				if (entity.HasComponent<DirectionComponent>()) {
					auto& dir = entity.GetComponent<DirectionComponent>();
					if (dir.direction == Direction::LEFT) {
						flip = Flip::HORIZONTAL;
					}
				}
				if (entity.HasComponent<CollisionComponent>()) {
					auto& collisionComponent = entity.GetComponent<CollisionComponent>();
					//TextureManager::drawRectangle(sprite->texture, sprite->source, Util::RectFromVec(transform.position, collisionComponent->collider.size), 0.0, flip);
					// Draw colliders for debug purposes
					engine::TextureManager::DrawRectangle(V2_int{ transform.position.x, transform.position.y }, V2_int{ collisionComponent.collider.size.x, collisionComponent.collider.size.y }, renderComponent.color);
				} else {
					engine::TextureManager::DrawRectangle(sprite.path, V2_int{ sprite.source.x, sprite.source.y }, V2_int{ sprite.source.w, sprite.source.h }, V2_int{ transform.position.x, transform.position.y }, V2_int{ sprite.source.w, sprite.source.h }, flip, 0);
				}
			} else {
				if (entity.HasComponent<CollisionComponent>()) {
					auto& collisionComponent = entity.GetComponent<CollisionComponent>();
					engine::TextureManager::DrawRectangle({ transform.position.x, transform.position.y }, { collisionComponent.collider.size.x, collisionComponent.collider.size.y }, renderComponent.color);
				}
			}
		}
	}
};