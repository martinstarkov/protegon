#pragma once

#include "Component.h"

#include <SDL.h>
#include <engine/renderer/TextureManager.h>
#include <engine/renderer/AABB.h>
#include <Utilities.h>

struct SpriteComponent {
	std::string path;
	AABB source;
	SDL_Texture* texture;
	V2_double spriteSize;
	SpriteComponent() : path{}, source{}, texture{ nullptr }, spriteSize{} {}
	SpriteComponent(std::string path, V2_double sprite_size) : path{ path }, spriteSize{ sprite_size } {
		Init();
	}
	void Init() {
		source = AABB{ {}, spriteSize };
		texture = &engine::TextureManager::Load(path, path);
	}
	~SpriteComponent() {
		// TODO: Rethink texture removal after spriteComponent is destroyed
		// Don't necessarily remove texture if other textures are using it, but if none are, remove it
		// TextureManager::removeTexture(_path); 
	}
};

// json serialization
inline void to_json(nlohmann::json& j, const SpriteComponent& o) {
	j["path"] = o.path;
	j["spriteSize"] = o.spriteSize;
}

inline void from_json(const nlohmann::json& j, SpriteComponent& o) {
	if (j.find("path") != j.end()) {
		o.path = j.at("path").get<std::string>();
	}
	if (j.find("spriteSize") != j.end()) {
		o.spriteSize = j.at("spriteSize").get<V2_double>();
	}
	o.Init();
}