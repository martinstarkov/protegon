#pragma once

#include <cstdint> // std::uint32_t

#include "math/Vector2.h"

struct SDL_Window;

namespace engine {

class Window {
public:
	Window() = default;
	Window(SDL_Window* window);
	Window(const char* title, const V2_int& position, const V2_int& size, std::uint32_t flags = 0);
	operator SDL_Window* () const;
	bool IsValid() const;
	SDL_Window* operator&() const;
	void Destroy();
private:
	SDL_Window* window{ nullptr };
};

} // namespace engine