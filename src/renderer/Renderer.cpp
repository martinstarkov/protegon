#include "Renderer.h"

#include <cassert> // assert

#include <SDL.h>

#include "utility/Log.h"
#include "renderer/SDLRenderer.h"
#include "core/SDLWindow.h"

namespace ptgn {

namespace draw {

void Init(int index, std::uint32_t flags) {
	assert(SDLWindow::Get().window_ != nullptr && "Cannot create renderer from nonexistent window");
	auto& renderer = SDLRenderer::Get().renderer_;
	renderer = SDL_CreateRenderer(SDLWindow::Get().window_, index, flags);
	if (!Exists()) {
		PrintLine(SDL_GetError());
		assert(!"Failed to create renderer");
	}
}

void Release() {
	auto& renderer = SDLRenderer::Get().renderer_;
	SDL_DestroyRenderer(renderer);
	renderer = nullptr;
}

bool Exists() {
	return SDLRenderer::Get().renderer_ != nullptr;
}

void Present() {
	assert(Exists() && "Cannot present nonexistent renderer");
	SDL_RenderPresent(SDLRenderer::Get().renderer_);
}

void Clear() {
	assert(Exists() && "Cannot clear nonexistent renderer");
	SDL_RenderClear(SDLRenderer::Get().renderer_);
}

void SetColor(const Color& color) {
	assert(Exists() && "Cannot set draw color for nonexistent renderer");
	SDL_SetRenderDrawColor(SDLRenderer::Get().renderer_, color.r, color.g, color.b, color.a);
}

void Point(const V2_int& point,
						 const Color& color) {
	assert(Exists() && "Cannot draw point with nonexistent renderer");
	SetColor(color);
	SDL_RenderDrawPoint(SDLRenderer::Get().renderer_, point.x, point.y);
}

void Line(const V2_int& origin,
						const V2_int& destination,
						const Color& color) {
	assert(Exists() && "Cannot draw line with nonexistent renderer");
	SetColor(color);
	SDL_RenderDrawLine(SDLRenderer::Get().renderer_, origin.x, origin.y, destination.x, destination.y);
}

void Circle(const V2_int& center,
						  const double radius,
						  const Color& color) {
	assert(Exists() && "Cannot draw circle with nonexistent renderer");

	SetColor(color);

	int r{ math::Round(radius) };
	V2_int position{ r, 0 };
	auto renderer = SDLRenderer::Get().renderer_;
	SDL_RenderDrawPoint(renderer, center.x + position.x, center.y + position.y);

	if (radius > 0) {
		SDL_RenderDrawPoint(renderer, position.x + center.x, -position.y + center.y);
		SDL_RenderDrawPoint(renderer, position.y + center.x, position.x + center.y);
		SDL_RenderDrawPoint(renderer, -position.y + center.x, position.x + center.y);
	}

	int P{ 1 - r };

	while (position.x > position.y) {
		position.y++;

		if (P <= 0) {
			P = P + 2 * position.y + 1;
		} else {
			position.x--;
			P = P + 2 * position.y - 2 * position.x + 1;
		}

		if (position.x < position.y) {
			break;
		}

		SDL_RenderDrawPoint(renderer, position.x + center.x, position.y + center.y);
		SDL_RenderDrawPoint(renderer, -position.x + center.x, position.y + center.y);
		SDL_RenderDrawPoint(renderer, position.x + center.x, -position.y + center.y);
		SDL_RenderDrawPoint(renderer, -position.x + center.x, -position.y + center.y);

		if (position.x != position.y) {
			SDL_RenderDrawPoint(renderer, position.y + center.x, position.x + center.y);
			SDL_RenderDrawPoint(renderer, -position.y + center.x, position.x + center.y);
			SDL_RenderDrawPoint(renderer, position.y + center.x, -position.x + center.y);
			SDL_RenderDrawPoint(renderer, -position.y + center.x, -position.x + center.y);
		}
	}
}

// Draws filled circle to the screen.
void SolidCircle(const V2_int& center,
							   const double radius,
							   const Color& color) {
	assert(Exists() && "Cannot draw solid circle with nonexistent renderer");
	SetColor(color);
	int r{ math::Round(radius) };
	int r_squared{ r * r };
	auto renderer = SDLRenderer::Get().renderer_;
	for (auto y{ -r }; y <= r; ++y) {
		auto y_squared{ y * y };
		auto y_position{ center.y + y };
		for (auto x{ -r }; x <= r; ++x) {
			if (x * x + y_squared <= r_squared) {
				SDL_RenderDrawPoint(renderer, center.x + x, y_position);
			}
		}
	}
}

void Rectangle(const V2_int& top_left,
							 const V2_int& size,
							 const Color& color) {
	assert(Exists() && "Cannot draw rectangle with nonexistent renderer");
	SetColor(color);
	SDL_Rect rect{ top_left.x, top_left.y, size.x, size.y };
	SDL_RenderDrawRect(SDLRenderer::Get().renderer_, &rect);
}

void SolidRectangle(const V2_int& top_left,
								  const V2_int& size,
								  const Color& color) {
	assert(Exists() && "Cannot draw solid rectangle with nonexistent renderer");
	SetColor(color);
	SDL_Rect rect{ top_left.x, top_left.y, size.x, size.y };
	SDL_RenderFillRect(SDLRenderer::Get().renderer_, &rect);
}

void Texture(const ptgn::Texture& texture,
						   const V2_int& texture_position,
						   const V2_int& texture_size,
						   const V2_int& source_position,
						   const V2_int& source_size) {
	assert(Exists() && "Cannot draw texture with nonexistent renderer");
	assert(texture.Exists() && "Cannot draw nonexistent texture");
	SDL_Rect* source{ NULL };
	SDL_Rect source_rectangle;
	if (!source_size.IsZero()) {
		source_rectangle = { source_position.x, source_position.y, source_size.x, source_size.y };
		source = &source_rectangle;
	}
	SDL_Rect destination{ texture_position.x, texture_position.y, texture_size.x, texture_size.y };
	SDL_RenderCopy(SDLRenderer::Get().renderer_, texture, source, &destination);
}

void Texture(const ptgn::Texture& texture,
						   const V2_int& texture_position,
						   const V2_int& texture_size,
						   const V2_int& source_position,
						   const V2_int& source_size,
						   const V2_int* center_of_rotation,
						   const double angle,
						   Flip flip) {
	assert(Exists() && "Cannot draw texture with nonexistent renderer");
	assert(texture.Exists() && "Cannot draw nonexistent texture");
	auto renderer = SDLRenderer::Get().renderer_;
	SDL_Rect* source{ NULL };
	SDL_Rect source_rectangle;
	if (!source_position.IsZero() && !source_size.IsZero()) {
		source_rectangle = { source_position.x, source_position.y, source_size.x, source_size.y };
		source = &source_rectangle;
	}
	SDL_Rect destination{ texture_position.x, texture_position.y, texture_size.x, texture_size.y };
	if (center_of_rotation != nullptr) {
		SDL_Point center{ center_of_rotation->x, center_of_rotation->y };
		SDL_RenderCopyEx(renderer, texture, source, &destination,
						 angle, &center, static_cast<SDL_RendererFlip>(static_cast<int>(flip)));
	} else {
		SDL_RenderCopyEx(renderer, texture, source, &destination,
						 angle, NULL, static_cast<SDL_RendererFlip>(static_cast<int>(flip)));
	}
}

} // namespace draw

} // namespace ptgn