#pragma once

#include "Component.h"

#include "SDL.h"
#include "../../TextureManager.h"
#include "../../AABB.h"

// Turn sprite component into sprite sheet that feeds into AnimationSystem and create custom systems for different states (MovementStateSystem, etc)

struct SpriteComponent : public Component<SpriteComponent> {
	std::string _path;
	SDL_Rect _source;
	SDL_Texture* _texture;
	Vec2D _spriteSize;
	SpriteComponent() : _path(), _source(), _texture(nullptr), _spriteSize() {}
	SpriteComponent(std::string path, Vec2D spriteSize) : _path(path), _spriteSize(spriteSize) {
		_source = AABB(Vec2D(), _spriteSize).AABBtoRect();
		_texture = TextureManager::load(_path);
	}
	friend std::ostream& operator<<(std::ostream& out, const SpriteComponent& obj) {
		out << "{" << std::endl;
		out << "_path: " << obj._path << ";" << std::endl;
		out << "_source: " << "{" << obj._source.x << "," << obj._source.y << "," << obj._source.w << "," << obj._source.h << "}" << ";" << std::endl;
		out << "_texture: " << obj._texture << ";" << std::endl;
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
	j["_path"] = o._path;
	j["_spriteSize"] = o._spriteSize;
}

inline void from_json(const nlohmann::json& j, SpriteComponent& o) {
	o = SpriteComponent(
		j.at("_path").get<std::string>(), 
		j.at("_spriteSize").get<Vec2D>()
	);
}