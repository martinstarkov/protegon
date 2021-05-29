#include "AnimationMap.h"

#include "renderer/TextureManager.h"
#include "math/Hasher.h"

namespace engine {

AnimationMap::AnimationMap(const char* sprite_sheet_path) : texture_key_{ sprite_sheet_path } {
	TextureManager::Load(texture_key_, sprite_sheet_path);
}

AnimationMap::~AnimationMap() {
	TextureManager::Unload(texture_key_);
}

void AnimationMap::Add(const char* animation_key, const Animation& animation) {
	auto key{ Hasher::HashCString(animation_key) };
	auto it{ animations_.find(key) };
	assert(it != std::end(animations_) && 
		   "Cannot add existing animation to sprite map");
	animations_.emplace(key, animation);
}

const Animation& AnimationMap::Get(const char* animation_key) {
	auto key{ Hasher::HashCString(animation_key) };
	auto it{ animations_.find(key) };
	assert(it != std::end(animations_) && "Animation not found in sprite map");
	return it->second;
}

} // namespace engine