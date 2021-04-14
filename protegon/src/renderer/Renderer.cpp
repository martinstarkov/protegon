#include "Renderer.h"

#include <SDL.h>

#include "renderer/text/Text.h"

namespace engine {

void Renderer::DrawTexture(const Texture& texture,
						   const V2_int& position,
						   const V2_int& size,
						   const V2_int source_position,
						   const V2_int source_size) const {
	SDL_Rect* source{ NULL };
	SDL_Rect source_rectangle;
	if (!source_position.IsZero() && !source_size.IsZero()) {
		source_rectangle = { source_position.x, source_position.y, source_size.x, source_size.y };
		source = &source_rectangle;
	}
	SDL_Rect destination{ position.x, position.y, size.x, size.y };
	SDL_RenderCopy(renderer, texture, source, &destination);
}

void Renderer::DrawText(const Text& text) const {
	text.Draw();
}

Renderer::Renderer(SDL_Renderer* renderer) : renderer{ renderer } {}

Renderer::Renderer(const Window& window, int renderer_index, std::uint32_t flags) : 
	renderer{ SDL_CreateRenderer(window, renderer_index, flags) } {
	if (!IsValid()) {
		SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Failed to create renderer: %s\n", SDL_GetError());
		assert(!true);
	}
}

Renderer::operator SDL_Renderer*() const {
	return renderer;
}

SDL_Renderer* Renderer::operator&() const {
	return renderer;
}

bool Renderer::IsValid() const {
	return renderer != nullptr;
}

void Renderer::Clear() {
	SDL_RenderClear(renderer);
}

void Renderer::Present() const {
	SDL_RenderPresent(renderer);
}

void Renderer::Destroy() {
	SDL_DestroyRenderer(renderer); 
	renderer = nullptr;
}

} // namespace engine