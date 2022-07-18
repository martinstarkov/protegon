#include "SpriteMap.h"

#include "managers/TextureManager.h"

namespace ptgn {

namespace animation {

SpriteMap::SpriteMap(const char* key, const char* path) : texture_key_{ math::Hash(key) } {
	managers::GetManager<managers::TextureManager>().Load(texture_key_, new Texture{ path });
}

SpriteMap::~SpriteMap() {
	managers::GetManager<managers::TextureManager>().Unload(texture_key_);
}

} // namespace animation

} // namespace engine