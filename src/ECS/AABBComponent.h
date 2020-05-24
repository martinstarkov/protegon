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
		_transformComponent = &entity->get<TransformComponent>(true);
		_sizeComponent = &entity->get<SizeComponent>(true);
		if (_rectangle) {
			_transformComponent->setPosition(_rectangle._position);
			_sizeComponent->setSize(_rectangle._size);
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