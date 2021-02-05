#pragma once

#include <cstdint> // std::uint32_t

#include "renderer/Window.h"

struct SDL_Renderer;

namespace engine {

struct Renderer {
	Renderer() = default;
	Renderer(SDL_Renderer* renderer);
	Renderer(const Window& window, int renderer_index = -1, std::uint32_t flags = 0);
	operator SDL_Renderer* () const;
	SDL_Renderer* operator&() const;
	bool IsValid() const;
	void Clear();
	void Present();
	void Destroy();
	SDL_Renderer* renderer = nullptr;
};

} // namespace engine