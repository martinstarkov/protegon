#include "TextureManager.h"

namespace ptgn {

namespace services {

interfaces::TextureManager& GetTextureManager() {
	// TODO: Change to return specific implementation.
	static interfaces::TextureManager texture_manager;
	return texture_manager;
}

} // namespace services

} // namespace ptgn