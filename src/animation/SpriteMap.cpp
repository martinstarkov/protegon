#include "SpriteMap.h"

#include "manager/TextureManager.h"
#include "math/Hash.h"

namespace ptgn {

namespace animation {

SpriteMap::SpriteMap(const char* texture_key, const char* map_path) : 
	texture_key_{ texture_key } {
	auto& texture_manager{ manager::Get<TextureManager>() };
	texture_manager.Load(math::Hash(texture_key_), map_path);
}

SpriteMap::~SpriteMap() {
	auto& texture_manager{ manager::Get<TextureManager>() };
	texture_manager.Unload(math::Hash(texture_key_));
}

} // namespace animation

} // namespace engine