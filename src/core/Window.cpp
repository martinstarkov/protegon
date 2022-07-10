#include "Window.h"

#include <SDL.h>

#include "utility/Log.h"

namespace ptgn {

Color Window::window_color_{ color::WHITE };
SDL_Window* Window::window_{ nullptr };

void Window::Create(const char* window_title, const V2_int& window_size, const V2_int& window_position, std::uint32_t window_flags) {
	window_ = SDL_CreateWindow(window_title, window_position.x, window_position.y, window_size.x, window_size.y, window_flags);
	if (window_ == nullptr) {
		PrintLine(SDL_GetError());
		assert(!"Failed to create window");
	}
}

void Window::Destroy() {
	SDL_DestroyWindow(window_);
	window_ = nullptr;
}

V2_int Window::GetSize() {
	V2_int size;
	assert(IsValid() && "Cannot get size of nonexistent window");
	SDL_GetWindowSize(window_, &size.x, &size.y);
	return size;
}

V2_int Window::GetOriginPosition() {
	V2_int origin;
	assert(IsValid() && "Cannot get origin position of nonexistent window");
	SDL_GetWindowPosition(window_, &origin.x, &origin.y);
	return origin;
}

const char* Window::GetTitle() {
	assert(IsValid() && "Cannot get title of nonexistent window");
	return SDL_GetWindowTitle(window_);
}

void Window::SetSize(const V2_int& new_size) {
	assert(IsValid() && "Cannot set size of nonexistent window");
	SDL_SetWindowSize(window_, new_size.x, new_size.y);
}

void Window::SetOriginPosition(const V2_int& new_origin) {
	assert(IsValid() && "Cannot set origin position of nonexistent window");
	SDL_SetWindowPosition(window_, new_origin.x, new_origin.y);
}

void Window::SetTitle(const char* new_title) {
	assert(IsValid() && "Cannot set title of nonexistent window");
	return SDL_SetWindowTitle(window_, new_title);
}

void Window::SetFullscreen(bool on) {
	assert(IsValid() && "Cannot set nonexistent window to fullscreen");
	SDL_SetWindowFullscreen(window_, on);
}

void Window::SetResizeable(bool on) {
	assert(IsValid() && "Cannot set nonexistent window to resizeable");
	SDL_SetWindowResizable(window_, static_cast<SDL_bool>(on));
}

} // namespace ptgn