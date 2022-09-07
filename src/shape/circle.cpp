#include "protegon/circle.h"

#include <SDL.h>

#include "core/game.h"

namespace ptgn {

namespace impl {

void DrawCircle(int x, int y, int r, const Color& color) {
	auto renderer{ global::GetGame().sdl.GetRenderer() };
	assert(renderer != nullptr && "Cannot draw circle with nonexistent renderer");
	
	SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

	V2_int p{ r, 0 };
	SDL_RenderDrawPoint(renderer, p.x + x, p.y + y);

	if (r > 0) {
		SDL_RenderDrawPoint(renderer, p.x + x, -p.y + y);
		SDL_RenderDrawPoint(renderer, p.y + x, p.x + y);
		SDL_RenderDrawPoint(renderer, -p.y + x, p.x + y);
	}

	int P{ 1 - r };

	while (p.x > p.y) {
		p.y++;

		if (P <= 0) {
			P = P + 2 * p.y + 1;
		} else {
			p.x--;
			P = P + 2 * p.y - 2 * p.x + 1;
		}

		if (p.x < p.y) {
			break;
		}

		SDL_RenderDrawPoint(renderer, p.x + x, p.y + y);
		SDL_RenderDrawPoint(renderer, -p.x + x, p.y + y);
		SDL_RenderDrawPoint(renderer, p.x + x, -p.y + y);
		SDL_RenderDrawPoint(renderer, -p.x + x, -p.y + y);

		if (p.x != p.y) {
			SDL_RenderDrawPoint(renderer, p.y + x, p.x + y);
			SDL_RenderDrawPoint(renderer, -p.y + x, p.x + y);
			SDL_RenderDrawPoint(renderer, p.y + x, -p.x + y);
			SDL_RenderDrawPoint(renderer, -p.y + x, -p.x + y);
		}
	}
}

void DrawSolidCircle(int x, int y, int r, const Color& color) {
	auto renderer{ global::GetGame().sdl.GetRenderer() };
	assert(renderer != nullptr && "Cannot draw solid circle with nonexistent renderer");

	SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
	
	int r2{ r * r };
	for (auto y{ -r }; y <= r; ++y) {
		auto y2{ y * y };
		auto y_pos{ y + y };
		for (auto x{ -r }; x <= r; ++x) {
			if (x * x + y2 <= r2) {
				SDL_RenderDrawPoint(renderer, x + x, y_pos);
			}
		}
	}
}

} // namespace impl

} // namespace ptgn