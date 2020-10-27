#pragma once

#include "Component.h"

#include "renderer/TextureManager.h"
#include "renderer/AABB.h"

struct SpriteComponent {
	std::string path;
	AABB source;
	engine::Texture texture;
	V2_int sprite_size;
	SpriteComponent() : path{}, source{}, texture{ nullptr }, sprite_size{ 0, 0 } {}
	SpriteComponent(std::string path, V2_int sprite_size) : path{ path }, sprite_size{ sprite_size } {
		Init();
	}
	void Init() {
		source = AABB{ { 0.0, 0.0 }, sprite_size };
		texture = &engine::TextureManager::Load(path, path);
	}
	~SpriteComponent() = default;
	// TODO: Rethink texture removal after spriteComponent is destroyed.
	// Don't necessarily remove texture if other textures are using it, but if none are, remove it.
	// TextureManager::removeTexture(_path);
};