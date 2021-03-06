#include "Window.h"

#include <SDL.h>

#include "debugging/Debug.h"

namespace ptgn {

Window& Window::Init(const char* title,
					 const V2_int& position,
					 const V2_int& size,
					 std::uint32_t flags) {
	auto& window{ GetInstance() };
	window.window_ = SDL_CreateWindow(title, position.x, position.y, size.x, size.y, flags);
	if (!window.IsValid()) {
		PrintLine("Failed to create window: ", SDL_GetError());
		abort();
	}
	return window;
}

Window::operator SDL_Window* () const {
	return window_;
}

bool Window::IsValid() const {
	return window_ != nullptr;
}

V2_int Window::GetSize() {
	V2_int size;
	SDL_GetWindowSize(GetInstance(), &size.x, &size.y);
	return size;
}

void Window::SetSize(const V2_int& new_size) {
	SDL_SetWindowSize(GetInstance(), new_size.x, new_size.y);
}

V2_int Window::GetOriginPosition() {
	V2_int origin;
	SDL_GetWindowPosition(GetInstance(), &origin.x, &origin.y);
	return origin;
}

void Window::SetOriginPosition(const V2_int& new_origin) {
	SDL_SetWindowPosition(GetInstance(), new_origin.x, new_origin.y);
}

const char* Window::GetTitle() {
	return SDL_GetWindowTitle(GetInstance());
}

void Window::SetTitle(const char* new_title) {
	return SDL_SetWindowTitle(GetInstance(), new_title);
}

void Window::SetFullscreen(bool on) {
	SDL_SetWindowFullscreen(GetInstance(), on);
}

void Window::SetResizeable(bool on) {
	SDL_SetWindowResizable(GetInstance(), static_cast<SDL_bool>(on));
}

SDL_Window* Window::operator&() const {
	return window_;
}

void Window::Destroy() {
	auto& window{ GetInstance() };
	SDL_DestroyWindow(window);
	window.window_ = nullptr;
}

} // namespace ptgn