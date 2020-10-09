#pragma once

#include <SDL.h>

namespace engine {

struct Renderer {
	Renderer() = default;
	Renderer(SDL_Renderer* renderer) : renderer{ renderer } {}
	operator SDL_Renderer*() const { return renderer; }
	SDL_Renderer* operator&() const { return renderer; }
	operator bool() const { return renderer != nullptr; }
	void Clear() { SDL_RenderClear(renderer); }
	void Present() { SDL_RenderPresent(renderer); }
	void Destroy() { SDL_DestroyRenderer(renderer); renderer = nullptr; }
	SDL_Renderer* renderer = nullptr;
};

} // namespace engine