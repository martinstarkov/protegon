#pragma once

#include "Component.h"

#include "renderer/TextureManager.h"
#include "renderer/AABB.h"
#include "renderer/SpriteMap.h"

struct SpriteComponent {
	engine::SpriteMap sprite_map;
	AABB current_sprite{};
	V2_double scale;
	SpriteComponent() = delete;
	SpriteComponent(const char* path, V2_double scale = { 1.0, 1.0 }, V2_double size = { 0.0, 0.0 }) :
		sprite_map{ path }, 
		scale{ scale } {
		current_sprite.size = size;
		Init();
	}
	void Init() {}
	~SpriteComponent() = default;
	// TODO: Rethink texture removal after SpriteComponent is destroyed.
	// Don't necessarily remove texture if other textures are using it, but if none are, remove it.
	// TextureManager::RemoveTexture(path);
};