#include "Texture.h"

#include "managers/TextureManager.h"
#include "math/Hash.h"

namespace ptgn {

namespace texture {

void Load(const char* texture_key, const char* texture_path) {
	auto& texture_manager{ internal::managers::GetManager<internal::managers::TextureManager>() };
	texture_manager.Load(math::Hash(texture_key), new internal::Texture(texture_path));
}

void Unload(const char* texture_key) {
	auto& texture_manager{ internal::managers::GetManager<internal::managers::TextureManager>() };
	texture_manager.Unload(math::Hash(texture_key));
}

bool Exists(const char* texture_key) {
	auto& texture_manager{ internal::managers::GetManager<internal::managers::TextureManager>() };
	return texture_manager.Has(math::Hash(texture_key));
}

} // namespace texture

} // namespace ptgn