#include "Window.h"

#include <SDL.h>

#include "core/Window.h"
#include "utility/Log.h"

#include "renderer/Renderer.h"
#include "Windows.h"

namespace ptgn {

namespace window {

void Init(const char* window_title, const V2_int& window_size, const V2_int& window_position, Flags window_flags) {
	auto& window = Window::Get().window_;
	window = SDL_CreateWindow(window_title, window_position.x, window_position.y, window_size.x, window_size.y, static_cast<std::uint32_t>(window_flags));
	if (window == nullptr) {
		PrintLine(SDL_GetError());
		assert(!"Failed to create window");
	}
}

void Release() {
	auto& window = Window::Get().window_;
	SDL_DestroyWindow(window);
	window = nullptr;
}

bool Exists() {
	return Window::Get().window_ != nullptr;
}

V2_int GetSize() {
	V2_int size;
	assert(Exists() && "Cannot get size of nonexistent window");
	SDL_GetWindowSize(Window::Get().window_, &size.x, &size.y);
	return size;
}

V2_int GetOriginPosition() {
	V2_int origin;
	assert(Exists() && "Cannot get origin position of nonexistent window");
	SDL_GetWindowPosition(Window::Get().window_, &origin.x, &origin.y);
	return origin;
}

const char* GetTitle() {
	assert(Exists() && "Cannot get title of nonexistent window");
	return SDL_GetWindowTitle(Window::Get().window_);
}

Color GetColor() {
	assert(Exists() && "Cannot get color of nonexistent window");
	return Window::Get().color_;
}

void SetSize(const V2_int& new_size) {
	assert(Exists() && "Cannot set size of nonexistent window");
	SDL_SetWindowSize(Window::Get().window_, new_size.x, new_size.y);
}

void SetOriginPosition(const V2_int& new_origin) {
	assert(Exists() && "Cannot set origin position of nonexistent window");
	SDL_SetWindowPosition(Window::Get().window_, new_origin.x, new_origin.y);
}

void SetTitle(const char* new_title) {
	assert(Exists() && "Cannot set title of nonexistent window");
	return SDL_SetWindowTitle(Window::Get().window_, new_title);
}

void SetFullscreen(window::Flags flag) {
	assert(Exists() && "Cannot set nonexistent window to fullscreen");
	assert(flag == window::Flags::FULLSCREEN_DESKTOP ||
		   flag == window::Flags::FULLSCREEN ||
		   flag == window::Flags::NONE);
	SDL_SetWindowFullscreen(Window::Get().window_, static_cast<std::uint32_t>(flag));
}

void SetResizeable(bool on) {
	assert(Exists() && "Cannot set nonexistent window to resizeable");
	SDL_SetWindowResizable(Window::Get().window_, static_cast<SDL_bool>(on));
}

void SetColor(const Color& new_color) {
	Window::Get().color_ = new_color;
}

void Maximize() {
	assert(Exists() && "Cannot maximize nonexistent window");
	SDL_MaximizeWindow(Window::Get().window_);
}

void Minimize() {
	assert(Exists() && "Cannot minimize nonexistent window");
	SDL_MinimizeWindow(Window::Get().window_);
}

void Show() {
	assert(Exists() && "Cannot show nonexistent window");
	SDL_ShowWindow(Window::Get().window_);
}

void Hide() {
	assert(Exists() && "Cannot hide nonexistent window");
	SDL_HideWindow(Window::Get().window_);
}

} // namespace window

} // namespace ptgn