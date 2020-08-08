#pragma once

#include "System.h"

#include "../../TextureManager.h"

class RenderSystem : public System<RenderComponent, TransformComponent> {
public:
	virtual void update() override final {
		for (auto& id : entities) {
			auto [renderComponent, transform] = getComponents(id);
			auto [sprite, collisionComponent] = getComponents<SpriteComponent, CollisionComponent>(id);
			if (sprite) {
				SDL_RendererFlip flip = SDL_FLIP_NONE;
				auto dir = manager->getComponent<DirectionComponent>(id);
				if (dir) {
					if (dir->direction == Direction::LEFT) {
						flip = SDL_FLIP_HORIZONTAL;
					}
				}
				if (collisionComponent) {
					//TextureManager::drawRectangle(sprite->texture, sprite->source, Util::RectFromVec(transform.position, collisionComponent->collider.size), 0.0, flip);
					// Draw colliders for debug purposes
					TextureManager::drawRectangle(transform.position, collisionComponent->collider.size, renderComponent.color);
				} else {
					TextureManager::drawRectangle(sprite->texture, sprite->source, Util::RectFromPoints(transform.position.x, transform.position.y, sprite->source.w, sprite->source.h), 0.0, flip);
				}
			} else {
				if (collisionComponent) {
					TextureManager::drawRectangle(transform.position, collisionComponent->collider.size, renderComponent.color);
				}
			}
		}
	}
};