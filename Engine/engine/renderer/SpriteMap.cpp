#include "SpriteMap.h"

#include <cassert> // assert

#include "renderer/Texture.h"
#include "renderer/TextureManager.h"
#include "utils/Hasher.h"

namespace engine {

SpriteMap::SpriteMap(const char* path) : path{ path } {
	TextureManager::Load(path, path);
}

void SpriteMap::AddAnimation(const char* name, const Animation& animation) {
	auto key = Hasher::HashCString(name);
	auto it = animations_.find(key);
	if (it == std::end(animations_)) {
		animations_.emplace(key, animation);
	} else {
		assert(!"Animation already exists in sprite map");
	}
}

Animation SpriteMap::GetAnimation(const std::string& name) {
	return GetAnimation(name.c_str());
}

Animation SpriteMap::GetAnimation(const char* name) {
	auto key = Hasher::HashCString(name);
	auto it = animations_.find(key);
	assert(it != std::end(animations_) && "Animation not found in sprite map");
	return it->second;
}

} // namespace engine