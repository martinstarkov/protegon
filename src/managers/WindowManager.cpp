#include "WindowManager.h"

#include "debugging/Debug.h"

namespace ptgn {

namespace internal {

namespace managers {

const id WindowManager::GetTargetWindowId() const {
	assert(Has(target_window_) && "Could not find a valid target window");
	return target_window_;
}

const Window& WindowManager::GetTargetWindow() const {
	assert(Has(target_window_) && "Could not find a valid target window");
	const auto window{ Get(target_window_) };
	assert(window != nullptr && "Target window is nonexistent");
	return *window;
}
Window& WindowManager::GetTargetWindow() {
	return const_cast<Window&>(const_cast<const WindowManager*>(this)->GetTargetWindow());
}

void WindowManager::SetTargetWindow(id window) {
	assert(Has(window) && "Cannot set target window to nonexistent window");
	target_window_ = window;
}

id WindowManager::GetFirstTargetWindow() {
	return target_window_;
}

const Renderer& WindowManager::GetTargetRenderer() const {
	assert(Has(target_window_) && "Could not find a valid target window");
	const auto window{ Get(target_window_) };
	assert(window != nullptr && "Cannot find an existing window to retrieve renderer from");
	const auto& renderer{ window->GetRenderer() };
	assert(renderer != nullptr && "Target renderer is nonexistent");
	return renderer;
}
Renderer& WindowManager::GetTargetRenderer() {
	return const_cast<Renderer&>(const_cast<const WindowManager*>(this)->GetTargetRenderer());
}

} // namespace managers

} // namespace internal

} // namespace ptgn