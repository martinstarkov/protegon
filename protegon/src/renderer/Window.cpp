#include "Window.h"

#include <SDL.h>

namespace engine {

Window::Window(SDL_Window* window) : window{ window } {}

Window::Window(const char* title, const V2_int& position, const V2_int& size, std::uint32_t flags) : 
	window{ SDL_CreateWindow(title, position.x, position.y, size.x, size.y, flags) } {
	if (!IsValid()) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create window: %s\n", SDL_GetError());
		assert(!true);
	}
}

Window::operator SDL_Window* () const {
	return window;
}

bool Window::IsValid() const {
	return window != nullptr;
}

SDL_Window* Window::operator&() const {
	return window;
}

void Window::Destroy() {
	SDL_DestroyWindow(window);
	window = nullptr;
}

} // namespace engine