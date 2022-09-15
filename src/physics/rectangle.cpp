#include "protegon/rectangle.h"

#include <SDL.h>

#include "core/game.h"

namespace ptgn {

namespace impl {

void DrawRectangle(int x, int y, int w, int h, const Color& color) {
	auto renderer{ global::GetGame().sdl.GetRenderer() };
	assert(renderer != nullptr && "Cannot draw rectangle with nonexistent renderer");
	SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
	SDL_Rect rect{ x, y, w, h };
	SDL_RenderDrawRect(renderer, &rect);
}

void DrawSolidRectangle(int x, int y, int w, int h, const Color& color) {
	auto renderer{ global::GetGame().sdl.GetRenderer() };
	assert(renderer != nullptr && "Cannot draw solid rectangle with nonexistent renderer");
	SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
	SDL_Rect rect{ x, y, w, h };
	SDL_RenderFillRect(renderer, &rect);
}

} // namespace impl

} // namespace ptgn