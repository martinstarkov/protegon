#include "WindowManager.h"

#include "managers/SDLManager.h"

namespace ptgn {

namespace internal {

namespace managers {

WindowManager::WindowManager() {
	GetSDLManager();
}

} // namespace managers

managers::WindowManager& GetWindowManager() {
	static managers::WindowManager window_manager;
	return window_manager;
}

} // namespace internal

} // namespace ptgn