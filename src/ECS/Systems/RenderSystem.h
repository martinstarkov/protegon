#pragma once

#include "System.h"

#include "../../TextureManager.h"

class RenderSystem : public System<RenderComponent, TransformComponent, SizeComponent> {
public:
	virtual void update() override {
		for (auto& entityID : entities) {
			Entity& e = getEntity(entityID);
			TransformComponent* transform = e.getComponent<TransformComponent>();
			SizeComponent* size = e.getComponent<SizeComponent>();
			SpriteComponent* sprite = e.getComponent<SpriteComponent>();
			if (sprite) {
				TextureManager::draw(sprite->texture, sprite->source, AABB(transform->position, size->size).AABBtoRect());
			} else {
				TextureManager::draw(AABB(transform->position, size->size).AABBtoRect());
			}
		}
	}
};