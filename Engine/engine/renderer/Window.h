#pragma once

struct SDL_Window;

namespace engine {

struct Window {
	Window() = default;
	Window(SDL_Window* window);
	SDL_Window* operator=(SDL_Window* window);
	operator SDL_Window* () const;
	operator bool() const;
	SDL_Window* operator&() const;
	void Destroy();
	SDL_Window* window = nullptr;
};

} // namespace engine