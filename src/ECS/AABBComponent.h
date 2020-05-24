#pragma once
#include "Components.h"
#include "SDL.h"
#include "../AABB.h"

class AABBComponent : public Component {
public:
	AABBComponent() {
		_transformComponent = nullptr;
		_sizeComponent = nullptr;
		_rectangle = {};
	}
	AABBComponent(AABB rectangle) : AABBComponent() {
		_rectangle = rectangle;
	}
	void init() override {
		if (entity->count<TransformComponent>() < 2) {
			_transformComponent = entity->add<TransformComponent>(_rectangle._position);
		}
		if (entity->count<SizeComponent>() < 2) {
			_sizeComponent = entity->add<SizeComponent>(_rectangle._size);
		}
		if (entity->count<TransformComponent>() == 2) {
			_transformComponent = entity->get<TransformComponent>();
		}
		if (entity->count<SizeComponent>() == 2) {
			_sizeComponent = entity->get<SizeComponent>();
		}
	}
	AABB getAABB() {
		return { Vec2D(_transformComponent->getPosition().x, _transformComponent->getPosition().y), Vec2D(_sizeComponent->getSize().x, _sizeComponent->getSize().y) };
	}
	SDL_Rect getRectangle() {
		return getAABB().AABBtoRect();
	}
private:
	TransformComponent* _transformComponent;
	SizeComponent* _sizeComponent;
	AABB _rectangle;
};