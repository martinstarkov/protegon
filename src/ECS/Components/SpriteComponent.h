#pragma once
#include "Component.h"
#include "SDL.h"
#include "../../TextureManager.h"
#include "../../AABB.h"

struct SpriteComponent : public Component<SpriteComponent> {
	const char* _path;
	SDL_Rect _source;
	SDL_Texture* _texture;
	SpriteComponent(const char* path = nullptr, AABB spriteRectangle = {}) : _path(path), _source(spriteRectangle.AABBtoRect()) {
		_texture = TextureManager::load(_path);
	}
	virtual ~SpriteComponent() {
		TextureManager::removeTexture(_path);
		_texture = nullptr;
	}
};

//ComponentID SpriteComponent::ID = 0;