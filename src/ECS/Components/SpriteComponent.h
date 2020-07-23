#pragma once

#include "Component.h"

#include "SDL.h"
#include "../../TextureManager.h"
#include "../../AABB.h"

// Turn sprite component into sprite sheet that feeds into AnimationSystem and create custom systems for different states (MovementStateSystem, etc)

struct SpriteComponent : public Component<SpriteComponent> {
	std::string path;
	SDL_Rect source;
	SDL_Texture* texture;
	Vec2D spriteSize;
	SpriteComponent() : path(), source(), texture(nullptr), spriteSize() {}
	SpriteComponent(std::string path, Vec2D spriteSize) : path(path), spriteSize(spriteSize) {
		source = AABB(Vec2D(), spriteSize).AABBtoRect();
		texture = TextureManager::load(path);
	}
	friend std::ostream& operator<<(std::ostream& out, const SpriteComponent& obj) {
		out << "{" << std::endl;
		out << "path: " << obj.path << ";" << std::endl;
		out << "source: " << "{" << obj.source.x << "," << obj.source.y << "," << obj.source.w << "," << obj.source.h << "}" << ";" << std::endl;
		out << "texture: " << obj.texture << ";" << std::endl;
		out << "spriteSize: " << obj.spriteSize << ";" << std::endl;
		out << "}" << std::endl;
		return out;
	}
	virtual ~SpriteComponent() override {
		// don't necessarily remove texture if other textures are using it
		// TODO: Rethink texture removal after spriteComponent is destroyed
		// TextureManager::removeTexture(_path); 
	}
};

// json serialization
inline void to_json(nlohmann::json& j, const SpriteComponent& o) {
	j["path"] = o.path;
	j["spriteSize"] = o.spriteSize;
}

inline void from_json(const nlohmann::json& j, SpriteComponent& o) {
	o = SpriteComponent(
		j.at("path").get<std::string>(), 
		j.at("spriteSize").get<Vec2D>()
	);
}