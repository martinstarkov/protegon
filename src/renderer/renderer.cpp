#include "protegon/renderer.h"

#include <SDL.h>

#include "core/game.h"

namespace ptgn {

namespace renderer {

void SetDrawColor(const Color& color) {
	auto renderer{ global::GetGame().sdl.GetRenderer() };
	SDL_SetRenderDrawColor(renderer.get(), color.r, color.g, color.b, color.a);
}

} // namespace renderer

} // namespace ptgn