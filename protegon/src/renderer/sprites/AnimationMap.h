#pragma once

#include <unordered_map> // std::unordered_map

#include "renderer/sprites/Animation.h"

namespace engine {

class AnimationMap {
public:
	AnimationMap() = delete;
	AnimationMap(const char* sprite_sheet_path);
	~AnimationMap();
	void Add(const char* animation_key, const Animation& animation);
	const Animation& Get(const char* animation_key);
private:
	std::unordered_map<std::size_t, Animation> animations_;
	const char* texture_key_{ nullptr };
};

} // namespace engine