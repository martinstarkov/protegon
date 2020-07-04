#pragma once

#include "Component.h"

#include "SDL.h"
#include "../../TextureManager.h"
#include "../../AABB.h"

// Turn sprite component into sprite sheet that feeds into AnimationSystem and create custom systems for different states (MovementStateSystem, etc)

struct SpriteComponent : public Component<SpriteComponent> {
	const char* _path;
	SDL_Rect _source;
	SDL_Texture* _texture;
	SpriteComponent(const char* path = nullptr, Vec2D spriteSize = Vec2D()) : _path(path), _source(AABB(Vec2D(), spriteSize).AABBtoRect()) {
		_texture = TextureManager::load(_path);
	}
	virtual ~SpriteComponent() override {
		// don't necessarily remove texture if other textures are using it
		// TODO: Rethink texture removal after spriteComponent is destroyed
		TextureManager::removeTexture(_path); 
	}
};