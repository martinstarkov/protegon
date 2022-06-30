#include "Window.h"

#include "managers/WindowManager.h"

namespace ptgn {

namespace window {

internal::managers::id Create(const char* title, const V2_int& size, const V2_int& position, int flags) {
	auto& window_manager{ internal::GetWindowManager() };
	static internal::managers::id window_count{ 0 };
	window_manager.Load(window_count, new internal::Window(title, size, position, flags));
	return window_count++;
}

void Destroy(internal::managers::id window) {
	auto& window_manager{ internal::GetWindowManager() };
	window_manager.Unload(window);
}

bool Exists(internal::managers::id window) {
	auto& window_manager{ internal::GetWindowManager() };
	return window_manager.Has(window);
}

V2_int GetSize(internal::managers::id window) {
	auto& window_manager{ internal::GetWindowManager() };
	return window_manager.Get(window)->GetSize();
}

V2_int GetOriginPosition(internal::managers::id window) {
	auto& window_manager{ internal::GetWindowManager() };
	return window_manager.Get(window)->GetOriginPosition();
}

const char* GetTitle(internal::managers::id window) {
	auto& window_manager{ internal::GetWindowManager() };
	return window_manager.Get(window)->GetTitle();
}

Color GetColor(internal::managers::id window) {
	auto& window_manager{ internal::GetWindowManager() };
	return window_manager.Get(window)->GetColor();
}

void SetSize(internal::managers::id window, const V2_int& new_size) {
	auto& window_manager{ internal::GetWindowManager() };
	window_manager.Get(window)->SetSize(new_size);
}

void SetOriginPosition(internal::managers::id window, const V2_int& new_origin) {
	auto& window_manager{ internal::GetWindowManager() };
	window_manager.Get(window)->SetOriginPosition(new_origin);
}

void SetTitle(internal::managers::id window, const char* new_title) {
	auto& window_manager{ internal::GetWindowManager() };
	window_manager.Get(window)->SetTitle(new_title);
}

void SetFullscreen(internal::managers::id window, bool state) {
	auto& window_manager{ internal::GetWindowManager() };
	window_manager.Get(window)->SetFullscreen(state);
}

void SetResizeable(internal::managers::id window, bool state) {
	auto& window_manager{ internal::GetWindowManager() };
	window_manager.Get(window)->SetResizeable(state);
}

void SetColor(internal::managers::id window, const Color& color) {
	auto& window_manager{ internal::GetWindowManager() };
	window_manager.Get(window)->SetColor(color);
}

} // namespace window

} // namespace ptgn