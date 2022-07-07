#pragma once

#include <unordered_map>

#include "math/Hash.h"
#include "animation/Animation.h"
#include "renderer/Renderer.h"

namespace ptgn {

namespace animation {

class SpriteMap {
public:
	SpriteMap() = default;
	SpriteMap(const Renderer& renderer, const char* key, const char* path);
	~SpriteMap();
	void AddAnimation(const char* name, const Animation& animation) {
		animations.emplace(math::Hash(name), animation);
	}
	void RemoveAnimation(const char* name) {
		animations.erase(math::Hash(name));
	}
	const Animation& GetAnimation(const char* name) {
		const auto animation = animations.find(math::Hash(name));
		assert(animation != animations.end() && "Cannot retrieve nonexistent animation from sprite map");
		return animation->second;
	}
	const std::size_t GetTextureKey() const {
		return texture_key_;
	}
private:
	std::size_t texture_key_{ 0 };
	std::unordered_map<std::size_t, Animation> animations;
};

} // namespace animation

} // namespace ptgn