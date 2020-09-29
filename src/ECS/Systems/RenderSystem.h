#pragma once

#include "System.h"

#include <TextureManager.h>

class RenderSystem : public ecs::System<RenderComponent, TransformComponent> {
public:
	virtual void Update() override final {
		for (auto& [entity, renderComponent, transform] : entities) {
			if (entity.HasComponent<SpriteComponent>()) {
				auto& sprite = entity.GetComponent<SpriteComponent>();
				SDL_RendererFlip flip = SDL_FLIP_NONE;
				if (entity.HasComponent<DirectionComponent>()) {
					auto& dir = entity.GetComponent<DirectionComponent>();
					if (dir.direction == Direction::LEFT) {
						flip = SDL_FLIP_HORIZONTAL;
					}
				}
				if (entity.HasComponent<CollisionComponent>()) {
					auto& collisionComponent = entity.GetComponent<CollisionComponent>();
					//TextureManager::drawRectangle(sprite->texture, sprite->source, Util::RectFromVec(transform.position, collisionComponent->collider.size), 0.0, flip);
					// Draw colliders for debug purposes
					TextureManager::drawRectangle(transform.position, collisionComponent.collider.size, renderComponent.color);
				} else {
					TextureManager::drawRectangle(sprite.texture, sprite.source, Util::RectFromPoints(transform.position.x, transform.position.y, sprite.source.w, sprite.source.h), 0.0, flip);
				}
			} else {
				if (entity.HasComponent<CollisionComponent>()) {
					auto& collisionComponent = entity.GetComponent<CollisionComponent>();
					TextureManager::drawRectangle(transform.position, collisionComponent.collider.size, renderComponent.color);
				}
			}
		}
	}
};