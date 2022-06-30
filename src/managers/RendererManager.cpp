#include "RendererManager.h"

#include "managers/SDLManager.h"
#include "managers/WindowManager.h"

namespace ptgn {

namespace internal {

namespace managers {

RendererManager::RendererManager() {
	GetSDLManager();
	GetWindowManager();
}

} // namespace managers

managers::RendererManager& GetRendererManager() {
	static managers::RendererManager renderer_manager;
	return renderer_manager;
}

} // namespace internal

} // namespace ptgn