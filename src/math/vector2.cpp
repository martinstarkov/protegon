#include "protegon/vector2.h"

#include <cassert> // assert

#include <SDL.h>

#include "core/global.h"

namespace ptgn {

namespace impl {

void DrawPoint(int x, int y, const Color& color) {
	auto renderer{ global::GetGame().systems.sdl.GetRenderer() };
	assert(renderer != nullptr && "Cannot draw point with nonexistent renderer");
	SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
	SDL_RenderDrawPoint(renderer, x, y);
}

} // namespace impl

} // namespace ptgn