#include "Renderer.h"

#include <SDL.h>

#include "debugging/Debug.h"

namespace ptgn {

namespace internal {

Renderer::Renderer(SDL_Window* window, int index, std::uint32_t flags) {
	assert(window != nullptr && "Cannot create Renderer from non-existent window");
	renderer_ = SDL_CreateRenderer(window, index, flags);
	if (renderer_ == nullptr) {
		debug::PrintLine(SDL_GetError());
		assert(!"Failed to create renderer");
	}
}

Renderer::~Renderer() {
	SDL_DestroyRenderer(renderer_);
	renderer_ = nullptr;
}

void Renderer::Present() {
	assert(renderer_ != nullptr && "Cannot present non-existent renderer");
	SDL_RenderPresent(renderer_);
}

void Renderer::Clear() {
	assert(renderer_ != nullptr && "Cannot clear non-existent renderer");
	SDL_RenderClear(renderer_);
}

void Renderer::SetDrawColor(const Color& color) const {
	assert(renderer_ != nullptr && "Cannot set draw color for non-existent renderer");
	SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
}

void Renderer::DrawPoint(const V2_int& point,
		   				 const Color& color) const {
	assert(renderer_ != nullptr && "Cannot draw point with non-existent renderer");
	SetDrawColor(color);
	SDL_RenderDrawPoint(renderer_, point.x, point.y);
}

void Renderer::DrawLine(const V2_int& origin,
					    const V2_int& destination,
					    const Color& color) const {
	assert(renderer_ != nullptr && "Cannot draw line with non-existent renderer");
	SetDrawColor(color);
	SDL_RenderDrawLine(renderer_, origin.x, origin.y, destination.x, destination.y);
}

void Renderer::DrawCircle(const V2_int& center,
			   			  const double radius,
			   			  const Color& color) const {
	assert(renderer_ != nullptr && "Cannot draw circle with non-existent renderer");
	
	SetDrawColor(color);

	int r{ math::Round(radius) };
	V2_int position{ r, 0 };

	SDL_RenderDrawPoint(renderer_, center.x + position.x, center.y + position.y);

	if (radius > 0) {
		SDL_RenderDrawPoint(renderer_, position.x + center.x, -position.y + center.y);
		SDL_RenderDrawPoint(renderer_, position.y + center.x, position.x + center.y);
		SDL_RenderDrawPoint(renderer_, -position.y + center.x, position.x + center.y);
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

		SDL_RenderDrawPoint(renderer_, position.x + center.x, position.y + center.y);
		SDL_RenderDrawPoint(renderer_, -position.x + center.x, position.y + center.y);
		SDL_RenderDrawPoint(renderer_, position.x + center.x, -position.y + center.y);
		SDL_RenderDrawPoint(renderer_, -position.x + center.x, -position.y + center.y);
		
		if (position.x != position.y) {
			SDL_RenderDrawPoint(renderer_, position.y + center.x, position.x + center.y);
			SDL_RenderDrawPoint(renderer_, -position.y + center.x, position.x + center.y);
			SDL_RenderDrawPoint(renderer_, position.y + center.x, -position.x + center.y);
			SDL_RenderDrawPoint(renderer_, -position.y + center.x, -position.x + center.y);
		}
	}
}

// Draws filled circle to the screen.
void Renderer::DrawSolidCircle(const V2_int& center,
				 			   const double radius,
				 			   const Color& color) const {
	assert(renderer_ != nullptr && "Cannot draw solid circle with non-existent renderer");
	SetDrawColor(color);
	int r{ math::Round(radius) };
	int r_squared{ r * r };
	for (auto y{ -r }; y <= r; ++y) {
		auto y_squared{ y * y };
		auto y_position{ center.y + y };
		for (auto x{ -r }; x <= r; ++x) {
			if (x * x + y_squared <= r_squared) {
				SDL_RenderDrawPoint(renderer_, center.x + x, y_position);
			}
		}
	}
}

void Renderer::DrawRectangle(const V2_int& top_left,
			   				 const V2_int& size,
			   				 const Color& color) const {
	assert(renderer_ != nullptr && "Cannot draw rectangle with non-existent renderer");
	SetDrawColor(color);
	SDL_Rect rect{ top_left.x, top_left.y, size.x, size.y };
	SDL_RenderDrawRect(renderer_, &rect);
}

void Renderer::DrawSolidRectangle(const V2_int& top_left,
								  const V2_int& size,
								  const Color& color) const {
	assert(renderer_ != nullptr && "Cannot draw solid rectangle with non-existent renderer");
	SetDrawColor(color);
	SDL_Rect rect{ top_left.x, top_left.y, size.x, size.y };
	SDL_RenderFillRect(renderer_, &rect);
}

Renderer::operator SDL_Renderer*() const {
	assert(renderer_ != nullptr && "Cannot cast nullptr renderer to SDL_Renderer");
	return renderer_;
}

} // namespace internal

} // namespace ptgn