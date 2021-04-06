#include "Renderer.h"

#include <SDL.h>

namespace engine {

Renderer::Renderer(SDL_Renderer* renderer) : renderer{ renderer } {}
Renderer::Renderer(const Window& window, int renderer_index, std::uint32_t flags) : renderer{ SDL_CreateRenderer(window, renderer_index, flags) } {
	if (!IsValid()) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create renderer: %s\n", SDL_GetError());
		assert(!true);
	}
}
Renderer::operator SDL_Renderer*() const { return renderer; }
SDL_Renderer* Renderer::operator&() const { return renderer; }
bool Renderer::IsValid() const { return renderer != nullptr; }
void Renderer::Clear() { SDL_RenderClear(renderer); }
void Renderer::Present() { SDL_RenderPresent(renderer); }
void Renderer::Destroy() { SDL_DestroyRenderer(renderer); renderer = nullptr; }

} // namespace engine