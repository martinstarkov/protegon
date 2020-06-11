#pragma once

#include "System.h"

#include "../../TextureManager.h"

class RenderSystem : public System<RenderComponent, TransformComponent, SizeComponent> {
public:
	virtual void update() override {
		//LOG_("Rendering[" << _entities.size() << "],");
		for (auto& entityID : _entities) {
			Entity& e = getEntity(entityID);
			TransformComponent* transform = e.getComponent<TransformComponent>();
			SizeComponent* size = e.getComponent<SizeComponent>();
			SpriteComponent* sprite = e.getComponent<SpriteComponent>();
			if (sprite) {
				TextureManager::draw(sprite->_texture, sprite->_source, AABB(transform->_position, size->_size).AABBtoRect());
			} else {
				TextureManager::draw(AABB(transform->_position, size->_size).AABBtoRect());
			}
		}
	}
};