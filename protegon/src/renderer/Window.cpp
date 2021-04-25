#include "Window.h"

#include <SDL.h>

#include "debugging/Debug.h"

namespace engine {

Window::Window(SDL_Window* window) : window_{ window } {}

Window::Window(const char* title, const V2_int& position, const V2_int& size, std::size_t display_index, std::uint32_t flags) : 
	window_{ SDL_CreateWindow(title, position.x, position.y, size.x, size.y, flags) },
	display_index_{ display_index } {
	if (!IsValid()) {
		PrintLine("Failed to create window: ", SDL_GetError());
		abort();
	}
}

Window::operator SDL_Window* () const {
	return window_;
}

bool Window::IsValid() const {
	return window_ != nullptr;
}

V2_int Window::GetSize() const {
	int width{ 0 };
	int height{ 0 };
	SDL_GetWindowSize(window_, &width, &height);
	return { width, height };
}

void Window::SetSize(const V2_int& new_size) {
	SDL_SetWindowSize(window_, new_size.x, new_size.y);
}

V2_int Window::GetPosition() const {
	int x{ 0 };
	int y{ 0 };
	SDL_GetWindowSize(window_, &x, &y);
	return { x, y };
}

void Window::SetPosition(const V2_int& new_position) {
	SDL_SetWindowSize(window_, new_position.x, new_position.y);
}

const char* Window::GetTitle() const {
	return SDL_GetWindowTitle(window_);
}

void Window::SetTitle(const char* new_title) {
	return SDL_SetWindowTitle(window_, new_title);
}

SDL_Window* Window::operator&() const {
	return window_;
}

void Window::Destroy() {
	SDL_DestroyWindow(window_);
	window_ = nullptr;
}

std::size_t Window::GetDisplayIndex() const {
	return display_index_;
}

} // namespace engine