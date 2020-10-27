#include "Renderer.h"

#include <SDL.h>

namespace engine {

Renderer::Renderer(SDL_Renderer* renderer) : renderer{ renderer } {}
Renderer::operator SDL_Renderer*() const { return renderer; }
SDL_Renderer* Renderer::operator&() const { return renderer; }
Renderer::operator bool() const { return renderer != nullptr; }
void Renderer::Clear() { SDL_RenderClear(renderer); }
void Renderer::Present() { SDL_RenderPresent(renderer); }
void Renderer::Destroy() { SDL_DestroyRenderer(renderer); renderer = nullptr; }

} // namespace engine