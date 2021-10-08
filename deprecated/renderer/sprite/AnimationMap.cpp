#include "AnimationMap.h"

#include "renderer/TextureManager.h"
#include "math/Math.h"

namespace ptgn {

AnimationMap::AnimationMap(const char* sprite_sheet_path) : texture_key_{ sprite_sheet_path } {
	TextureManager::Load(texture_key_, sprite_sheet_path);
}

AnimationMap::~AnimationMap() {
	TextureManager::Unload(texture_key_);
}

void AnimationMap::Add(const char* animation_key, const Animation& animation) {
	const auto key{ math::Hash(animation_key) };
	auto it{ animations_.find(key) };
	assert(it != std::end(animations_) && 
		   "Cannot add existing animation to sprite map");
	animations_.emplace(key, animation);
}

const Animation& AnimationMap::Get(const char* animation_key) {
	const auto key{ math::Hash(animation_key) };
	auto it{ animations_.find(key) };
	assert(it != std::end(animations_) && "Animation not found in sprite map");
	return it->second;
}

} // namespace ptgn