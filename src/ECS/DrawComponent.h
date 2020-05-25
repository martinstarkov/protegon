#pragma once
#include "Components.h"
#include "SDL.h"
#include "../TextureManager.h"

class DrawComponent : public Component {
public:
	DrawComponent() {
		_aabbComponent = nullptr;
		_spriteComponent = nullptr;
		_colorComponent = nullptr;
	}
	void init() override {
		if (entity->has<AABBComponent>()) {
			_aabbComponent = entity->get<AABBComponent>();
		} else if (entity->has<TransformComponent>() || entity->has<SizeComponent>()) {
			if (entity->has<HitboxComponent>() && entity->count<TransformComponent>() == 1 && entity->count<SizeComponent>() == 1) {
				_aabbComponent = entity->get<HitboxComponent>();
			} else {
				_aabbComponent = entity->add<AABBComponent>();
			}
		}
		if (_aabbComponent) {
			add(_aabbComponent);
			if (entity->has<SpriteComponent>()) {
				_spriteComponent = entity->get<SpriteComponent>();
				add(_spriteComponent);
				if (!_aabbComponent->getAABB()._size) {
					_aabbComponent->get<SizeComponent>()->setSize(Vec2D(_spriteComponent->getSource().w, _spriteComponent->getSource().h));
				}
			}
			if (entity->has<ColorComponent>()) {
				_colorComponent = entity->get<ColorComponent>();
				add(_colorComponent);
			}
		}
	}
	void draw() override {
		if (_aabbComponent) {
			if (_spriteComponent) {
				TextureManager::draw(_spriteComponent->getTexture(), _spriteComponent->getSource(), _aabbComponent->getRectangle());
			} else {
				if (_colorComponent) {
					TextureManager::draw(_aabbComponent->getRectangle(), _colorComponent->getColor());
				} else {
					TextureManager::draw(_aabbComponent->getRectangle());
				}
			}
		}
	}
private:
	AABBComponent* _aabbComponent;
	SpriteComponent* _spriteComponent;
	ColorComponent* _colorComponent;
};