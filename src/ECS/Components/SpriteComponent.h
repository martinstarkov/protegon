#pragma once

#include "Component.h"

#include "SDL.h"
#include "../../TextureManager.h"
#include "../../Vec2D.h"
#include "../../AABB.h"

// Turn sprite component into sprite sheet that feeds into AnimationSystem and create custom systems for different states (MovementStateSystem, etc)

struct SpriteComponent : public Component<SpriteComponent> {
	std::string path;
	SDL_Rect source;
	SDL_Texture* texture;
	Vec2D spriteSize;
	SpriteComponent() : path(), source(), texture(nullptr), spriteSize() {}
	SpriteComponent(std::string path, Vec2D spriteSize) : path(path), spriteSize(spriteSize) {
		init();
	}
	void init() {
		source = Util::RectFromVec(Vec2D(), spriteSize);
		texture = TextureManager::load(path);
	}
	virtual ~SpriteComponent() override {
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
		o.spriteSize = j.at("spriteSize").get<Vec2D>();
	}
	o.init();
}