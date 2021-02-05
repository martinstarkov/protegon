#pragma once

#include <cstdint> // std::uint32_t

#include "math/Vector2.h"

struct SDL_Window;

namespace engine {

struct Window {
	Window() = default;
	Window(SDL_Window* window);
	Window(const char* title, const V2_int& position, const V2_int& size, std::uint32_t flags = 0);
	SDL_Window* operator=(SDL_Window* window);
	operator SDL_Window* () const;
	bool IsValid() const;
	SDL_Window* operator&() const;
	void Destroy();
	SDL_Window* window = nullptr;
};

} // namespace engine