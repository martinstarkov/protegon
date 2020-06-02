#pragma once
#include "System.h"
#include "./TextureManager.h"

class RenderSystem : public System<RenderComponent, TransformComponent, SizeComponent> {
private:
	using BaseType = System<RenderComponent, TransformComponent, SizeComponent>;
public:
	virtual void update() override {
		std::cout << "Rendering[" << _entities.size() << "],";
		for (auto& pair : _entities) {
			TransformComponent* transform = pair.second->getComponent<TransformComponent>();
			SizeComponent* size = pair.second->getComponent<SizeComponent>();
			SpriteComponent* sprite = pair.second->getComponent<SpriteComponent>();
			if (sprite) {
				TextureManager::draw(sprite->_texture, sprite->_source, AABB(transform->_position, size->_size).AABBtoRect());
			} else {
				TextureManager::draw(AABB(transform->_position, size->_size).AABBtoRect());
			}
		}
	}
};