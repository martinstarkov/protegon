#pragma once
#include "Components.h"
#include "SDL.h"
#include "../TextureManager.h"

class DrawComponent : public Component {
public:
	DrawComponent() {
		_aabbComponent = nullptr;
		_spriteComponent = nullptr;
	}
	void init() override {
		_aabbComponent = &entity->get<AABBComponent>(true);
		if (entity->has<SpriteComponent>()) {
			_spriteComponent = &entity->get<SpriteComponent>();
			if (!_aabbComponent->getAABB()._size) {
				entity->get<SizeComponent>().setSize(Vec2D(_spriteComponent->getSource().w, _spriteComponent->getSource().h));
			}
		}
	}
	void draw() override {
		if (_spriteComponent) {
			TextureManager::draw(_spriteComponent->getTexture(), _spriteComponent->getSource(), _aabbComponent->getRectangle());
		} else {
			if (entity->has<ColorComponent>()) {
				TextureManager::draw(_aabbComponent->getRectangle(), entity->get<ColorComponent>().getColor());
			} else {
				TextureManager::draw(_aabbComponent->getRectangle());
			}
		}
	}
private:
	AABBComponent* _aabbComponent;
	SpriteComponent* _spriteComponent;
};