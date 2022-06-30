#include "Texture.h"

#include "managers/TextureManager.h"
#include "managers/WindowManager.h"
#include "math/Hash.h"

namespace ptgn {

namespace texture {

void Load(const char* texture_key, const char* texture_path, internal::managers::id window) {
	auto& texture_manager{ internal::GetTextureManager() };
	auto& window_manager{ internal::GetWindowManager() };
	assert(window_manager.Has(window) && "cannot load texture into non-existent window");
	internal::Window* window_instance = window_manager.Get(window);
	texture_manager.Load(math::Hash(texture_key), new internal::Texture(window_instance->GetRenderer(), texture_path));
}

void Unload(const char* texture_key) {
	auto& texture_manager{ internal::GetTextureManager() };
	texture_manager.Unload(math::Hash(texture_key));
}

bool Exists(const char* texture_key) {
	const auto& texture_manager{ internal::GetTextureManager() };
	return texture_manager.Has(math::Hash(texture_key));
}

} // namespace texture

} // namespace ptgn