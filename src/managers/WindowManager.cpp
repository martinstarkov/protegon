#include "WindowManager.h"

#include "debugging/Debug.h"

namespace ptgn {

namespace managers {

const id WindowManager::GetTargetWindowId() const {
	assert(Has(target_window_) && "Could not find a valid target window");
	return target_window_;
}

void WindowManager::SetTargetWindow(const id window) {
	assert(Has(window) && "Cannot set target window to nonexistent window");
	target_window_ = window;
}

id WindowManager::GetFirstTargetWindow() {
	return target_window_;
}

} // namespace managers

} // namespace ptgn