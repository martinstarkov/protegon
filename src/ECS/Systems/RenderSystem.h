#pragma once

#include "System.h"

#include "../../TextureManager.h"

class RenderSystem : public System<RenderComponent, TransformComponent> {
public:
	virtual void update() override final {
		for (auto& id : entities) {
			Entity e = Entity(id, manager);
			TransformComponent* transform = e.getComponent<TransformComponent>();
			SpriteComponent* sprite = e.getComponent<SpriteComponent>();
			CollisionComponent* collider = e.getComponent<CollisionComponent>();
			DirectionComponent* direction = e.getComponent<DirectionComponent>();
			if (sprite) {
				SDL_RendererFlip flip = SDL_FLIP_NONE;
				if (direction) {
					if (direction->direction == Direction::LEFT) {
						flip = SDL_FLIP_HORIZONTAL;
					}
				}
				if (collider) {
					//TextureManager::draw(sprite->texture, sprite->source, Util::RectFromVec(transform->position, collider->collider.size), 0.0, flip);
					TextureManager::draw(Util::RectFromVec(transform->position, collider->collider.size));
				} else {
					TextureManager::draw(sprite->texture, sprite->source, Util::RectFromVec(transform->position, Vec2D(sprite->source.w, sprite->source.h)), 0.0, flip);
				}
			} else {
				if (collider) {
					TextureManager::draw(Util::RectFromVec(transform->position, collider->collider.size));
				}
			}
		}
	}
};