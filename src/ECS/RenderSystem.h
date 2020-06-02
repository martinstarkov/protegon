#pragma once
#include "System.h"
#include "../TextureManager.h"

class RenderSystem : public System<TransformComponent, SizeComponent, SpriteComponent> {
private:
	using BaseType = System<TransformComponent, SizeComponent, SpriteComponent>;
public:
	RenderSystem(Manager* manager) : BaseType(manager) {}
	virtual void update() override {
		std::cout << "Rendering[" << _components.size() << "],";
		for (auto& componentTuple : _components) {
			TransformComponent* transform = std::get<TransformComponent*>(componentTuple);
			SizeComponent* size = std::get<SizeComponent*>(componentTuple);
			SpriteComponent* sprite = std::get<SpriteComponent*>(componentTuple);
			TextureManager::draw(sprite->_texture, sprite->_source, AABB(transform->_position, size->_size).AABBtoRect());
		}
	}
};