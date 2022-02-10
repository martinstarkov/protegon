#include "Texture.h"

#include "texture/TextureManager.h"
#include "math/Hash.h"

namespace ptgn {

namespace texture {

void Load(const char* texture_key, const char* texture_path) {
	auto& texture_manager{ services::GetTextureManager() };
	texture_manager.LoadTexture(math::Hash(texture_key), texture_path);
}

void Unload(const char* texture_key) {
	auto& texture_manager{ services::GetTextureManager() };
	texture_manager.UnloadTexture(math::Hash(texture_key));
}

bool Exists(const char* texture_key) {
	const auto& texture_manager{ services::GetTextureManager() };
	return texture_manager.HasTexture(math::Hash(texture_key));
}

} // namespace texture

} // namespace ptgn