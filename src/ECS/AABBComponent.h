#pragma once
#include "Components.h"
#include "SDL.h"
#include "../AABB.h"

class AABBComponent : public Component {
public:
	AABBComponent() {
		_transformComponent = nullptr;
		_sizeComponent = nullptr;
		_type = nullptr;
		_rectangle = {};
	}
	AABBComponent(AABB rectangle, AABBComponent* type = nullptr) : AABBComponent() {
		_rectangle = rectangle;
		_type = type;
	}
	void init() override {
		if (!_type) { // for AABB inherit previous components
			if (entity->has<TransformComponent>()) {
				for (auto c : entity->getComponents<TransformComponent>()) {
					if (!c->isChild()) {
						_transformComponent = c;
						break;
					} else {
						_transformComponent = entity->get<TransformComponent>();
					}
				}
				if (!_transformComponent) {
					_transformComponent = entity->add<TransformComponent>();
				}
			} else {
				_transformComponent = entity->add<TransformComponent>();
			}
			if (entity->has<SizeComponent>()) {
				for (auto c : entity->getComponents<SizeComponent>()) {
					if (!c->isChild()) {
						_sizeComponent = c;
						break;
					} else {
						_sizeComponent = entity->get<SizeComponent>();
					}
				}
				if (!_sizeComponent) {
					_sizeComponent = entity->add<SizeComponent>();
				}
			} else {
				_sizeComponent = entity->add<SizeComponent>();
			}
		} else { // for hitbox always add new components
			entity->addGroup(Groups::hitboxes);
			_transformComponent = entity->add<TransformComponent>();
			_sizeComponent = entity->add<SizeComponent>();
		}
		entity->addGroup(Groups::drawables);
		if (_rectangle) { // if rectangle given, set positions appropriately
			_transformComponent->setPosition(_rectangle._position);
			_sizeComponent->setSize(_rectangle._size);
		}
		add(_transformComponent);
		add(_sizeComponent);
	}
	AABB getAABB() {
		return { get<TransformComponent>()->getPosition(), get<SizeComponent>()->getSize() };
	}
	SDL_Rect getRectangle() {
		return getAABB().AABBtoRect();
	}
private:
	AABBComponent* _type;
	TransformComponent* _transformComponent;
	SizeComponent* _sizeComponent;
	AABB _rectangle;
};