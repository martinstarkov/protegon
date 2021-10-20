#include "Renderer.h"

#include <cassert> // assert

#include <SDL.h>

#include "math/Math.h"
#include "window/WindowManager.h"
#include "texture/TextureManager.h"
#include "debugging/Debug.h"

namespace ptgn {

namespace impl {

SDLRenderer::SDLRenderer(SDL_Window* window, int index, std::uint32_t flags) {
	assert(window != nullptr && "Cannot create SDLRenderer from non-existent window");
	renderer_ = SDL_CreateRenderer(window, 0, 0);
	if (renderer_ == nullptr) {
		debug::PrintLine("Failed to create renderer: ", SDL_GetError());
		abort();
	}
}

SDLRenderer::~SDLRenderer() {
	SDL_DestroyRenderer(renderer_);
	renderer_ = nullptr;
}

void SDLRenderer::Present() {
	assert(renderer_ != nullptr && "Cannot present non-existent renderer");
	SDL_RenderPresent(renderer_);
}

void SDLRenderer::Clear() {
	assert(renderer_ != nullptr && "Cannot clear non-existent renderer");
	SDL_RenderClear(renderer_);
}

void SDLRenderer::SetDrawColor(const Color& color) const {
	SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
}

// Draws a texture to the screen.
void SDLRenderer::DrawTexture(const char* texture_key,
						  const V2_int& position,
						  const V2_int& size,
						  const V2_int& source_position,
						  const V2_int& source_size) const {
	assert(renderer_ != nullptr && "Cannot draw texture with non-existent sdl renderer");
	auto& sdl_texture_manager{ impl::GetSDLTextureManager() };
	auto texture{ sdl_texture_manager.GetTexture(math::Hash(texture_key)) };
	assert(texture != nullptr && "Cannot draw texture which is not loaded in the sdl texture manager");
	SDL_Rect* source{ NULL };
	SDL_Rect source_rectangle;
	if (!source_size.IsZero()) {
		source_rectangle = { source_position.x, source_position.y, source_size.x, source_size.y };
		source = &source_rectangle;
	}
	SDL_Rect destination{ position.x, position.y, size.x, size.y };
	SDL_RenderCopy(renderer_, texture.get(), source, &destination);
}

// Draws a texture to the screen. Allows for rotation and flip.
void SDLRenderer::DrawTexture(const char* texture_key,
						  const V2_int& position,
						  const V2_int& size,
						  const V2_int& source_position,
						  const V2_int& source_size,
						  const V2_int* center_of_rotation,
						  const double angle,
						  Flip flip) const {
	assert(renderer_ != nullptr && "Cannot draw texture with non-existent sdl renderer");
	// // TODO: Get texture from texture key and texture manager.
	// auto texture{ TextureManager::GetTexture(texture_key) };
	// assert(texture.IsValid() && "Cannot draw texture that has been uninitialized or destroyed");
	// SDL_Rect* source{ NULL };
	// SDL_Rect source_rectangle;
	// if (!source_position.IsZero() && !source_size.IsZero()) {
	// 	source_rectangle = { source_position.x, source_position.y, source_size.x, source_size.y };
	// 	source = &source_rectangle;
	// }
	// SDL_Rect destination{ position.x, position.y, size.x, size.y };
	// if (center_of_rotation != nullptr) {
	// 	SDL_Point center{ center_of_rotation->x, center_of_rotation->y };
	// 	SDL_RenderCopyEx(renderer_, 
	// 					 texture, 
	// 					 source, 
	// 					 &destination, 
	// 					 angle, 
	// 					 &center, 
	// 					 static_cast<SDL_RendererFlip>(static_cast<int>(flip))
	// 	);
	// } else {
	// 	SDL_RenderCopyEx(renderer_,
	// 					 texture,
	// 					 source, 
	// 					 &destination, 
	// 					 angle, 
	// 					 NULL, 
	// 					 static_cast<SDL_RendererFlip>(static_cast<int>(flip))
	// 	);
	// }
}

// Draws text to the screen.
void SDLRenderer::DrawText(const char* text_key,
		    		   const V2_int& position,
		    		   const V2_int& size) const {
	assert(renderer_ != nullptr && "Cannot draw text with non-existent sdl renderer");
	// // TODO: Get text from text key and text manager.
	// auto& text{  }
	// assert(text.GetTexture() != nullptr && "Cannot draw non-existent text");
	// SDL_Rect destination{ position.x, position.y, size.x, size.y };
	// SDL_RenderCopy(renderer_, text.GetTexture(), NULL, &destination);
}

// Draws a user interface element.
void SDLRenderer::DrawUI(const char* ui_key,
		 			 const V2_int& position,
		 			 const V2_int& size) const {
	assert(renderer_ != nullptr && "Cannot draw ui with non-existent sdl renderer");
	// TODO: Implement ui manager fetch for ui element and call its draw function here.
}

// Draws a point on the screen.
void SDLRenderer::DrawPoint(const V2_int& point,
		   				const Color& color) const {
	assert(renderer_ != nullptr && "Cannot draw point with non-existent sdl renderer");
	SetDrawColor(color);
	SDL_RenderDrawPoint(renderer_, point.x, point.y);
}

// Draws line to the screen.
void SDLRenderer::DrawLine(const V2_int& origin,
					   const V2_int& destination,
					   const Color& color) const {
	assert(renderer_ != nullptr && "Cannot draw line with non-existent sdl renderer");
	SetDrawColor(color);
	SDL_RenderDrawLine(renderer_, origin.x, origin.y, destination.x, destination.y);
}

// Draws hollow circle to the screen.
void SDLRenderer::DrawCircle(const V2_int& center,
			   			 const double radius,
			   			 const Color& color) const {
	assert(renderer_ != nullptr && "Cannot draw circle with non-existent sdl renderer");
	
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
void SDLRenderer::DrawSolidCircle(const V2_int& center,
				 			  const double radius,
				 			  const Color& color) const {
	assert(renderer_ != nullptr && "Cannot draw solid circle with non-existent sdl renderer");
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

// Draws hollow rectangle to the screen.
void SDLRenderer::DrawRectangle(const V2_int& center,
			   				const V2_int& size,
			   				const Color& color) const {
	assert(renderer_ != nullptr && "Cannot draw rectangle with non-existent sdl renderer");
	SetDrawColor(color);
	SDL_Rect rect{ center.x - size.x / 2, center.y - size.y / 2, size.x, size.y };
	SDL_RenderDrawRect(renderer_, &rect);
}

// Draws filled rectangle to the screen.
void SDLRenderer::DrawSolidRectangle(const V2_int& center,
								 const V2_int& size,
								 const Color& color) const {
	assert(renderer_ != nullptr && "Cannot draw solid rectangle with non-existent sdl renderer");
	SetDrawColor(color);
	SDL_Rect rect{ center.x - size.x / 2, center.y - size.y / 2, size.x, size.y };
	SDL_RenderFillRect(renderer_, &rect);
}

SDLRenderer& GetSDLRenderer() {
	static SDLRenderer sdl_renderer{ GetSDLWindowManager().window_, -1, 0 };
	return sdl_renderer;
}

} // namespace impl

namespace services {

interfaces::Renderer& GetRenderer() {
	return impl::GetSDLRenderer();
}

} // namespace services

} // namespace ptgn