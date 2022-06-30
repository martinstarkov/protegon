#include "TextureManager.h"

#include "core/SDLManager.h"

namespace ptgn {

namespace internal {

namespace managers {

TextureManager::TextureManager() {
	GetSDLManager();
}

} // namespace managers

managers::TextureManager& GetTextureManager() {
	static managers::TextureManager texture_manager;
	return texture_manager;
}

} // namespace internal

} // namespace ptgn