#pragma once

#include <unordered_map> // std::unordered_map

#include "renderer/Animation.h"

namespace engine {

class SpriteMap {
public:
	SpriteMap() = delete;
	SpriteMap(const char* path);
	void AddAnimation(const char* name, const Animation& animation);
	Animation GetAnimation(const char* name);
	Animation GetAnimation(const std::string& name);
	const char* path;
private:
	std::unordered_map<std::size_t, Animation> animations_;
};

} // namespace engine