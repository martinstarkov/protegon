#include "ScreenRenderer.h"

#include <SDL.h>
#include <cassert>

#include "core/Window.h"
#include "renderer/text/Text.h"
#include "renderer/Surface.h"
#include "renderer/TextureManager.h"
#include "debugging/Debug.h"

namespace ptgn {

void ScreenRenderer::DrawTexture(const Texture& texture, 
						   const V2_int& position, 
						   const V2_int& size, 
						   const V2_int source_position, 
						   const V2_int source_size) {
	auto& renderer{ GetInstance() };
	assert(renderer.IsValid() && "Cannot draw texture with destroyed or uninitialized renderer");
	assert(texture.IsValid() && "Cannot draw texture that has been uninitialized or destroyed");
	SDL_Rect* source{ NULL };
	SDL_Rect source_rectangle;
	if (!source_size.IsZero()) {
		source_rectangle = { source_position.x, source_position.y, source_size.x, source_size.y };
		source = &source_rectangle;
	}
	SDL_Rect destination{ position.x, position.y, size.x, size.y };
	SDL_RenderCopy(renderer, texture, source, &destination);
}

void ScreenRenderer::DrawTexture(const char* texture_key,
						   const V2_int& position,
						   const V2_int& size,
						   const V2_int source_position,
						   const V2_int source_size) {
	auto texture{ TextureManager::GetTexture(texture_key) };
	assert(texture.IsValid() && "Cannot draw texture that has been uninitialized or destroyed");
	DrawTexture(texture, position, size, source_position, source_size);
}

void ScreenRenderer::DrawTexture(const char* texture_key, 
						   const V2_int& position, 
						   const V2_int& size, 
						   const V2_int source_position, 
						   const V2_int source_size,
						   const V2_int* center_of_rotation, 
						   const double angle,
						   Flip flip) {
	auto& renderer{ GetInstance() };
	assert(renderer.IsValid() && "Cannot draw texture with destroyed or uninitialized renderer");
	auto texture{ TextureManager::GetTexture(texture_key) };
	assert(texture.IsValid() && "Cannot draw texture that has been uninitialized or destroyed");
	SDL_Rect* source{ NULL };
	SDL_Rect source_rectangle;
	if (!source_position.IsZero() && !source_size.IsZero()) {
		source_rectangle = { source_position.x, source_position.y, source_size.x, source_size.y };
		source = &source_rectangle;
	}
	SDL_Rect destination{ position.x, position.y, size.x, size.y };
	if (center_of_rotation != nullptr) {
		SDL_Point center{ center_of_rotation->x, center_of_rotation->y };
		SDL_RenderCopyEx(renderer, 
						 texture, 
						 source, 
						 &destination, 
						 angle, 
						 &center, 
						 static_cast<SDL_RendererFlip>(static_cast<int>(flip))
		);
	} else {
		SDL_RenderCopyEx(renderer,
						 texture,
						 source, 
						 &destination, 
						 angle, 
						 NULL, 
						 static_cast<SDL_RendererFlip>(static_cast<int>(flip))
		);
	}

}

void ScreenRenderer::DrawText(const Text& text,
							  const V2_int& position,
							  const V2_int& size) {
	auto& renderer{ GetInstance() };
	assert(renderer.IsValid() && "Cannot draw text with destroyed or uninitialized renderer");
	assert(text.GetTexture().IsValid() && "Cannot draw text that has been uninitialized or destroyed");
	SDL_Rect destination{ position.x, position.y, size.x, size.y };
	SDL_RenderCopy(renderer, text.GetTexture(), NULL, &destination);
}

void ScreenRenderer::DrawPoint(const V2_int& point, const Color& color) {
	auto& renderer{ GetInstance() };
	assert(renderer.IsValid() && "Cannot draw point with destroyed or uninitialized renderer");
	SetDrawColor(color);
	SDL_RenderDrawPoint(renderer, point.x, point.y);
	SetDrawColor();
}

void ScreenRenderer::DrawLine(const V2_int& origin, 
						const V2_int& destination, 
						const Color& color) {
	auto& renderer{ GetInstance() };
	assert(renderer.IsValid() && "Cannot draw line with destroyed or uninitialized renderer");
	SetDrawColor(color);
	SDL_RenderDrawLine(renderer, origin.x, origin.y, destination.x, destination.y);
	SetDrawColor();
}

void ScreenRenderer::DrawCircle(const V2_int& center, 
						  const double radius, 
						  const Color& color) {
	auto& renderer{ GetInstance() };
	assert(renderer.IsValid() && "Cannot draw circle with destroyed or uninitialized renderer");
	SetDrawColor(color);
	int r{ math::Round(radius) };
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
	SetDrawColor();
}

void ScreenRenderer::DrawSolidCircle(const V2_int& center,
							   const double radius,
							   const Color& color) {
	auto& renderer{ GetInstance() };
	assert(renderer.IsValid() && "Cannot draw solid circle with destroyed or uninitialized renderer");
	SetDrawColor(color);
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
	SetDrawColor();
}

void ScreenRenderer::DrawRectangle(const V2_int& position,
							 const V2_int& size,
							 const Color& color) {
	auto& renderer{ GetInstance() };
	assert(renderer.IsValid() && "Cannot draw rectangle with destroyed or uninitialized renderer");
	SetDrawColor(color);
	SDL_Rect rect{ position.x, position.y, size.x, size.y };
	SDL_RenderDrawRect(renderer, &rect);
	SetDrawColor();
}

void ScreenRenderer::DrawSolidRectangle(const V2_int& position,
								  const V2_int& size,
								  const Color& color) {
	auto& renderer{ GetInstance() };
	assert(renderer.IsValid() && "Cannot draw solid rectangle with destroyed or uninitialized renderer");
	SetDrawColor(color);
	SDL_Rect rect{ position.x, position.y, size.x, size.y };
	SDL_RenderFillRect(renderer, &rect);
	SetDrawColor();
}

Texture ScreenRenderer::CreateTexture(const Surface& surface) {
	return { GetInstance(), surface };
}

Texture ScreenRenderer::CreateTexture(const V2_int& size,
									  std::uint32_t format,
									  TextureAccess texture_access) {
	return { GetInstance(), size, format, texture_access };
}

ScreenRenderer& ScreenRenderer::Init(const Window& window,
						 int renderer_index, 
						 std::uint32_t flags) {
	auto& renderer{ GetInstance() };
	renderer.renderer_ = SDL_CreateRenderer(window, renderer_index, flags);
	if (!renderer.IsValid()) {
		PrintLine("Failed to create renderer: ", SDL_GetError());
		abort();
	}
	return renderer;
}

ScreenRenderer::operator SDL_Renderer*() const {
	return renderer_;
}

SDL_Renderer* ScreenRenderer::operator&() const {
	return renderer_;
}

bool ScreenRenderer::IsValid() const {
	return renderer_ != nullptr;
}

void ScreenRenderer::Clear() {
	SDL_RenderClear(GetInstance());
}

void ScreenRenderer::Present() {
	SDL_RenderPresent(GetInstance());
}

void ScreenRenderer::Destroy() {
	auto& renderer{ GetInstance() };
	SDL_DestroyRenderer(renderer);
	renderer.renderer_ = nullptr;
}

void ScreenRenderer::SetDrawColor(const Color& color) {
	SDL_SetRenderDrawColor(GetInstance(), color.r, color.g, color.b, color.a);
}

} // namespace ptgn