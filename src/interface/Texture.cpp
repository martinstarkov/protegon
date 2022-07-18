#include "Texture.h"

#include "manager/TextureManager.h"
#include "math/Hash.h"

namespace ptgn {

namespace texture {

void Load(const char* texture_key, const char* texture_path) {
	auto& texture_manager{ manager::Get<TextureManager>() };
	texture_manager.Load(math::Hash(texture_key), texture_path);
}

void Unload(const char* texture_key) {
	auto& texture_manager{ manager::Get<TextureManager>() };
	texture_manager.Unload(math::Hash(texture_key));
}

} // namespace texture

} // namespace ptgn