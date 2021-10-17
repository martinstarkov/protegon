#include "Texture.h"

#include "texture/TextureManager.h"

namespace ptgn {

namespace texture {

void Load(const char* texture_key, const char* texture_path) {
	auto& texture_manager{ services::GetTextureManager() };
	texture_manager.LoadTexture(texture_key, texture_path);
}

void Unload(const char* texture_key) {
	auto& texture_manager{ services::GetTextureManager() };
	texture_manager.UnloadTexture(texture_key);
}

} // namespace texture

} // namespace ptgn