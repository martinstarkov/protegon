#pragma once

#include "System.h"

#include "../../TextureManager.h"

class RenderSystem : public System<RenderComponent, TransformComponent> {
public:
	virtual void update() override final {
		for (auto& id : entities) {
			Entity e = Entity(id, manager);
			TransformComponent* transform = e.getComponent<TransformComponent>();
			SizeComponent* size = e.getComponent<SizeComponent>();
			SpriteComponent* sprite = e.getComponent<SpriteComponent>();
			DirectionComponent* direction = e.getComponent<DirectionComponent>();
			if (sprite) {
				SDL_RendererFlip flip = SDL_FLIP_NONE;
				if (direction) {
					if (direction->direction == Direction::LEFT) {
						flip = SDL_FLIP_HORIZONTAL;
					}
				}
				if (size) {
					TextureManager::draw(sprite->texture, sprite->source, AABB(transform->position, size->size).AABBtoRect(), 0.0, flip);
				} else {
					TextureManager::draw(sprite->texture, sprite->source, AABB(transform->position, Vec2D(sprite->source.w, sprite->source.h)).AABBtoRect(), 0.0, flip);
				}
			} else {
				if (size) {
					TextureManager::draw(AABB(transform->position, size->size).AABBtoRect());
				}
			}
		}
	}
};