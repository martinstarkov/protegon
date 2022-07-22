#pragma once

#include "animation/Animation.h"
#include "manager/ResourceManager.h"

namespace ptgn {

namespace animation {

class SpriteMap : public manager::ResourceManager<Animation> {
public:
	SpriteMap() = default;
	SpriteMap(const char* texture_key, const char* map_path);
	~SpriteMap();
	const char* GetTextureKey() const { return texture_key_; }
private:
	const char* texture_key_{ 0 };
};

} // namespace animation

} // namespace ptgn