#pragma once

#include <SDL.h>

namespace engine {

struct Window {
	Window() = default;
	Window(SDL_Window* window) : window{ window } {}
	SDL_Window* operator=(SDL_Window* window) { this->window = window; return this->window; }
	operator SDL_Window*() const { return window; }
	operator bool() const { return window != nullptr; }
	SDL_Window* operator&() const { return window; }
	void Destroy() { SDL_DestroyWindow(window); window = nullptr; }
	SDL_Window* window = nullptr;
};

} // namespace engine