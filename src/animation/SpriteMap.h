#pragma once

#include "animation/Animation.h"
#include "managers/ResourceManager.h"

namespace ptgn {

namespace animation {

class SpriteMap : public managers::ResourceManager<Animation> {
public:
	SpriteMap() = default;
	SpriteMap(const char* texture_key, const char* map_path);
	~SpriteMap();
	std::size_t GetTextureKey() const { return texture_key_; }
private:
	std::size_t texture_key_{ 0 };
};

} // namespace animation

} // namespace ptgn