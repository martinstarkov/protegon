#include "Renderer.h"

#include <SDL.h>
#include <cassert>

#include "core/Engine.h"

#include "renderer/text/Text.h"
#include "math/Math.h"

namespace engine {

void Renderer::DrawTexture(const Texture& texture, 
						   const V2_int& position, 
						   const V2_int& size, 
						   const V2_int source_position, 
						   const V2_int source_size, 
						   std::size_t display_index) {
	auto renderer{ Engine::GetDisplay(display_index).second };
	assert(renderer.IsValid() && "Cannot draw texture with destroyed or uninitialized renderer");
	assert(texture.IsValid() && "Cannot draw texture that has been uninitialized or destroyed");
	SDL_Rect* source{ NULL };
	SDL_Rect source_rectangle;
	if (!source_position.IsZero() && !source_size.IsZero()) {
		source_rectangle = { source_position.x, source_position.y, source_size.x, source_size.y };
		source = &source_rectangle;
	}
	SDL_Rect destination{ position.x, position.y, size.x, size.y };
	SDL_RenderCopy(renderer, texture, source, &destination);
}

void Renderer::DrawText(const Text& text, 
						std::size_t display_index) {
	auto renderer{ Engine::GetDisplay(display_index).second };
	assert(renderer.IsValid() && "Cannot draw text with destroyed or uninitialized renderer");
	assert(text.GetTexture().IsValid() && "Cannot draw text that has been uninitialized or destroyed");
	const V2_int position{ text.GetPosition() };
	const V2_int size{ text.GetArea() };
	SDL_Rect destination{ position.x, position.y, size.x, size.y };
	SDL_RenderCopy(renderer, text.GetTexture(), NULL, &destination);
}

void Renderer::DrawPoint(const V2_int& point, 
						 const Color& color,
						 std::size_t display_index) {
	auto renderer{ Engine::GetDisplay(display_index).second };
	assert(renderer.IsValid() && "Cannot draw point with destroyed or uninitialized renderer");
	renderer.SetDrawColor(color);
	SDL_RenderDrawPoint(renderer, point.x, point.y);
	renderer.SetDrawColor();
}

void Renderer::DrawLine(const V2_int& origin, 
						const V2_int& destination, 
						const Color& color,
						std::size_t display_index) {
	auto renderer{ Engine::GetDisplay(display_index).second };
	assert(renderer.IsValid() && "Cannot draw line with destroyed or uninitialized renderer");
	renderer.SetDrawColor(color);
	SDL_RenderDrawLine(renderer, origin.x, origin.y, destination.x, destination.y);
	renderer.SetDrawColor();
}

void Renderer::DrawCircle(const V2_int& center, 
						  const double radius, 
						  const Color& color,
						  std::size_t display_index) {
	auto renderer{ Engine::GetDisplay(display_index).second };
	assert(renderer.IsValid() && "Cannot draw circle with destroyed or uninitialized renderer");
	renderer.SetDrawColor(color);

	int r{ engine::math::Round(radius) };
	V2_int position{ r, 0 };
	// Printing the initial point on the axes
	// after translation 
	SDL_RenderDrawPoint(renderer, center.x + position.x, center.y + position.y);
	// When radius is zero only a single 
	// point will be printed 
	if (radius > 0) {
		SDL_RenderDrawPoint(renderer, position.x + center.x, -position.y + center.y);
		SDL_RenderDrawPoint(renderer, position.y + center.x, position.x + center.y);
		SDL_RenderDrawPoint(renderer, -position.y + center.x, position.x + center.y);
	}

	// Initialising the value of P 
	int P{ 1 - r };
	while (position.x > position.y) {
		position.y++;

		// Mid-point is inside or on the perimeter 
		if (P <= 0)
			P = P + 2 * position.y + 1;

		// Mid-point is outside the perimeter 
		else {
			position.x--;
			P = P + 2 * position.y - 2 * position.x + 1;
		}

		// All the perimeter points have already been printed 
		if (position.x < position.y)
			break;

		// Printing the generated point and its reflection 
		// in the other octants after translation 
		SDL_RenderDrawPoint(renderer, position.x + center.x, position.y + center.y);
		SDL_RenderDrawPoint(renderer, -position.x + center.x, position.y + center.y);
		SDL_RenderDrawPoint(renderer, position.x + center.x, -position.y + center.y);
		SDL_RenderDrawPoint(renderer, -position.x + center.x, -position.y + center.y);

		// If the generated point is on the line x = y then  
		// the perimeter points have alreadposition.y been printed 
		if (position.x != position.y) {
			SDL_RenderDrawPoint(renderer, position.y + center.x, position.x + center.y);
			SDL_RenderDrawPoint(renderer, -position.y + center.x, position.x + center.y);
			SDL_RenderDrawPoint(renderer, position.y + center.x, -position.x + center.y);
			SDL_RenderDrawPoint(renderer, -position.y + center.x, -position.x + center.y);
		}
	}
	renderer.SetDrawColor();
}

void Renderer::DrawSolidCircle(const V2_int& center,
							   const double radius,
							   const Color& color, 
							   std::size_t display_index) {
	auto renderer{ Engine::GetDisplay(display_index).second };
	assert(renderer.IsValid() && "Cannot draw solid circle with destroyed or uninitialized renderer");
	renderer.SetDrawColor(color);
	int r{ math::Round(radius) };
	int r_squared{ r * r };
	for (auto y{ -r }; y <= r; ++y) {
		auto y_squared{ y * y };
		auto y_position{ center.y + y };
		for (auto x{ -r }; x <= r; ++x) {
			if (x * x + y_squared <= r_squared) {
				SDL_RenderDrawPoint(renderer, center.x + x, y_position);
			}
		}
	}
	renderer.SetDrawColor();
}

void Renderer::DrawRectangle(const V2_int& position,
				   const V2_int& size,
				   const Color& color,
				   std::size_t display_index) {
	auto renderer{ Engine::GetDisplay(display_index).second };
	assert(renderer.IsValid() && "Cannot draw rectangle with destroyed or uninitialized renderer");
	renderer.SetDrawColor(color);
	SDL_Rect rect{ position.x, position.y, size.x, size.y };
	SDL_RenderDrawRect(renderer, &rect);
	renderer.SetDrawColor();
}

void Renderer::DrawSolidRectangle(const V2_int& position,
						const V2_int& size,
						const Color& color,
						std::size_t display_index) {
	auto renderer{ Engine::GetDisplay(display_index).second };
	assert(renderer.IsValid() && "Cannot draw solid rectangle with destroyed or uninitialized renderer");
	renderer.SetDrawColor(color);
	SDL_Rect rect{ position.x, position.y, size.x, size.y };
	SDL_RenderFillRect(renderer, &rect);
	renderer.SetDrawColor();
}

Renderer::Renderer(SDL_Renderer* renderer) : renderer_{ renderer } {}

Renderer::Renderer(const Window& window, int renderer_index, std::uint32_t flags) : 
	renderer_{ SDL_CreateRenderer(window, renderer_index, flags) },
	display_index_{ window.GetDisplayIndex() } {
	if (!IsValid()) {
		SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Failed to create renderer: %s\n", SDL_GetError());
		assert(!true);
	}
}

Renderer::operator SDL_Renderer*() const {
	return renderer_;
}

SDL_Renderer* Renderer::operator&() const {
	return renderer_;
}

bool Renderer::IsValid() const {
	return renderer_ != nullptr;
}

void Renderer::Clear() {
	SDL_RenderClear(renderer_);
}

void Renderer::Present() const {
	SDL_RenderPresent(renderer_);
}

void Renderer::Destroy() {
	SDL_DestroyRenderer(renderer_);
	renderer_ = nullptr;
}

std::size_t Renderer::GetDisplayIndex() const {
	return display_index_;
}

void Renderer::SetDrawColor(const Color& color) {
	SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
}

} // namespace engine