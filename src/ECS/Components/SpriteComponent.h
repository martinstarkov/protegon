#pragma once

#include "Component.h"

#include "SDL.h"
#include "../../TextureManager.h"
#include "../../AABB.h"

struct SpriteComponent : public Component<SpriteComponent> {
	const char* _path;
	SDL_Rect _source;
	unsigned int _sprites;
	SpriteComponent(const char* path = nullptr, Vec2D spriteSize = Vec2D(), unsigned int sprites = 1) : _path(path), _source(AABB(Vec2D(), spriteSize).AABBtoRect()), _sprites(sprites) {
		TextureManager::load(_path);
	}
	virtual ~SpriteComponent() override {
		//TextureManager::removeTexture(_path); // don't necessarily remove texture if other textures are using it
	}
};