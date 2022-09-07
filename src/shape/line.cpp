#include "protegon/line.h"

#include <SDL.h>

#include "core/game.h"

namespace ptgn {

namespace impl {

void DrawLine(int x1, int y1, int x2, int y2, const Color& color) {
	auto renderer{ global::GetGame().sdl.GetRenderer() };
	assert(renderer != nullptr && "Cannot draw line with nonexistent renderer");

	SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

	SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
}

} // namespace impl

} // namespace ptgn