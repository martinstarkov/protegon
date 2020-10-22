#pragma once

#include "Component.h"

#include <SDL.h>
#include <engine/renderer/TextureManager.h>
#include <engine/renderer/AABB.h>

struct SpriteComponent {
	std::string path;
	AABB source;
	SDL_Texture* texture;
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

// json serialization
inline void to_json(nlohmann::json& j, const SpriteComponent& o) {
	j["path"] = o.path;
	j["sprite_size"] = o.sprite_size;
}

inline void from_json(const nlohmann::json& j, SpriteComponent& o) {
	if (j.find("path") != j.end()) {
		o.path = j.at("path").get<std::string>();
	}
	if (j.find("sprite_size") != j.end()) {
		o.sprite_size = j.at("sprite_size").get<V2_int>();
	}
	o.Init();
}