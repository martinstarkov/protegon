#include "Window.h"

#include "debugging/Debug.h"

#include <SDL.h>

namespace ptgn {

namespace internal {

Window::Window(const char* window_title, const V2_int& window_size, const V2_int& window_position, std::uint32_t window_flags) {
	window_ = SDL_CreateWindow(window_title, window_position.x, window_position.y, window_size.x, window_size.y, window_flags);
	if (window_ == nullptr) {
		debug::PrintLine(SDL_GetError());
		assert(!"Failed to create window");
	}
	renderer_ = Renderer(window_, 0, 0);
}

Window::~Window() {
	SDL_DestroyWindow(window_);
	window_ = nullptr;
}

bool Window::Exists() const {
	return window_ != nullptr;
}

V2_int Window::GetSize() const {
	V2_int size;
	assert(window_ != nullptr && "Cannot get size of non-existent window");
	SDL_GetWindowSize(window_, &size.x, &size.y);
	return size;
}

V2_int Window::GetOriginPosition() const {
	V2_int origin;
	assert(window_ != nullptr && "Cannot get origin position of non-existent window");
	SDL_GetWindowPosition(window_, &origin.x, &origin.y);
	return origin;
}

const char* Window::GetTitle() const {
	assert(window_ != nullptr && "Cannot get title of non-existent window");
	return SDL_GetWindowTitle(window_);
}

Color Window::GetColor() const {
	return color;
}

void Window::SetSize(const V2_int& new_size) {
	assert(window_ != nullptr && "Cannot set size of non-existent window");
	SDL_SetWindowSize(window_, new_size.x, new_size.y);
}

void Window::SetOriginPosition(const V2_int& new_origin) {
	assert(window_ != nullptr && "Cannot set origin position of non-existent window");
	SDL_SetWindowPosition(window_, new_origin.x, new_origin.y);
}

void Window::SetTitle(const char* new_title) {
	assert(window_ != nullptr && "Cannot set title of non-existent window");
	return SDL_SetWindowTitle(window_, new_title);
}

void Window::SetFullscreen(bool on) {
	assert(window_ != nullptr && "Cannot set non-existent window to fullscreen");
	SDL_SetWindowFullscreen(window_, on);
}

void Window::SetResizeable(bool on) {
	assert(window_ != nullptr && "Cannot set non-existent window to resizeable");
	SDL_SetWindowResizable(window_, static_cast<SDL_bool>(on));
}

void Window::SetColor(const Color& new_color) {
	color = new_color;
}

Window::operator SDL_Window*() const {
	return window_;
}

const Renderer& Window::GetRenderer() const {
	assert(renderer_ != nullptr && "Cannot return nullptr renderer");
	return renderer_;
}

} // namespace internal

} // namespace ptgn