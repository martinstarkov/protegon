#include "Window.h"

#include "window/WindowManager.h"
#include "renderer/Renderer.h"

namespace ptgn {

namespace window {

void Create(const char* title, const V2_int& size, const V2_int& position, int flags) {
	auto& window_manager{ services::GetWindowManager() };
	window_manager.CreateWindow(title, size, position, flags);
}

void Destroy() {
	auto& window_manager{ services::GetWindowManager() };
	window_manager.DestroyWindow();
}

void Present() {
	auto& renderer{ services::GetRenderer() };
	renderer.Present();
}

void Clear() {
	auto& renderer{ services::GetRenderer() };
	renderer.Clear();
}

void SetColor(const Color& color) {
	auto& renderer{ services::GetRenderer() };
	renderer.SetDrawColor(color);
	renderer.Clear();
}

V2_int GetSize() {
	auto& window_manager{ services::GetWindowManager() };
	return window_manager.GetWindowSize();
}

V2_int GetOriginPosition() {
	auto& window_manager{ services::GetWindowManager() };
	return window_manager.GetWindowOriginPosition();
}

const char* GetTitle() {
	auto& window_manager{ services::GetWindowManager() };
	return window_manager.GetWindowTitle();
}

void SetSize(const V2_int& new_size) {
	auto& window_manager{ services::GetWindowManager() };
	window_manager.SetWindowSize(new_size);
}

void SetOriginPosition(const V2_int& new_origin) {
	auto& window_manager{ services::GetWindowManager() };
	window_manager.SetWindowOriginPosition(new_origin);
}

void SetTitle(const char* new_title) {
	auto& window_manager{ services::GetWindowManager() };
	window_manager.SetWindowTitle(new_title);
}

void SetFullscreen(bool on) {
	auto& window_manager{ services::GetWindowManager() };
	window_manager.SetWindowFullscreen(on);
}

void SetResizeable(bool on) {
	auto& window_manager{ services::GetWindowManager() };
	window_manager.SetWindowResizeable(on);
}

} // namespace window

} // namespace ptgn