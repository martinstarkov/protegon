#include "protegon/rectangle.h"

#include <SDL.h>

#include "protegon/line.h"
#include "core/game.h"

namespace ptgn {

namespace impl {

void DrawRectangle(int x, int y, int w, int h, const Color& color) {
	auto renderer{ global::GetGame().sdl.GetRenderer() };
	assert(renderer != nullptr && "Cannot draw rectangle with nonexistent renderer");
	SDL_Rect rect{ x, y, w, h };
	if (color.a != 255)
		SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
	SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
	SDL_RenderDrawRect(renderer, &rect);
}

void DrawThickRectangle(int x, int y, int w, int h, const Color& color, std::uint8_t pixel_width) {
	auto renderer{ global::GetGame().sdl.GetRenderer() };
	assert(renderer != nullptr && "Cannot draw thick rectangle with nonexistent renderer");
	assert(pixel_width >= 1 && "Cannot draw rectangle with thickness below 1 pixel");
	int x2 = x + w;
	int y2 = y + h;

	int wh;

	// Special case: thick "point"
	if ((x == x2) && (y == y2)) {
		wh = pixel_width / 2;
		DrawSolidRectangleImpl(renderer, x - wh, y - wh, x2 + wh, y2 + wh, color);
		return;
	}

	if (color.a != 255)
		SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
	SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

	DrawThickLineImpl(renderer, x,  y,  x2 - 1, y,  (double)pixel_width);
	DrawThickLineImpl(renderer, x2 - 1, y,  x2 - 1, y2 - 1, (double)pixel_width);
	DrawThickLineImpl(renderer, x2 - 1, y2 - 1, x,  y2 - 1, (double)pixel_width);
	DrawThickLineImpl(renderer, x,  y2 - 1, x,  y,  (double)pixel_width);
}

void DrawSolidRectangle(int x, int y, int w, int h, const Color& color) {
	auto renderer{ global::GetGame().sdl.GetRenderer() };
	assert(renderer != nullptr && "Cannot draw solid rectangle with nonexistent renderer");
	DrawSolidRectangleImpl(renderer, x, y, x + w, y + h, color);
}

void DrawSolidRectangleImpl(SDL_Renderer* renderer, int x1, int y1, int x2, int y2, const Color& color) {
	
	int tmp;
	SDL_Rect rect;

	/*
	* Test for special cases of straight lines or single point
	*/
	if (x1 == x2) {
		if (y1 == y2) {
			DrawPixel(renderer, x1, y1, color);
			return;
		} else {
			DrawVerticalLine(renderer, x1, y1, y2, color);
			return;
		}
	} else {
		if (y1 == y2) {
			DrawHorizontalLine(renderer, x1, x2, y1, color);
			return;
		}
	}

	if (x1 > x2) {
		tmp = x1;
		x1 = x2;
		x2 = tmp;
	}

	if (y1 > y2) {
		tmp = y1;
		y1 = y2;
		y2 = tmp;
	}

	rect.x = x1;
	rect.y = y1;
	rect.w = x2 - x1;
	rect.h = y2 - y1;

	if (color.a != 255)
		SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
	SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
	SDL_RenderFillRect(renderer, &rect);
}

} // namespace impl

} // namespace ptgn