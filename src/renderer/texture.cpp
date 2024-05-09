#include "protegon/texture.h"

#include <SDL.h>
#include <SDL_image.h>

#include <cassert> // assert

#include "protegon/log.h"
#include "protegon/file.h"
#include "core/global.h"

namespace ptgn {

Texture::Texture(const char* image_path) {
	assert(*image_path && "Empty image path?");
	assert(FileExists(image_path) && "Nonexistent image path?");
	auto surface{ IMG_Load(image_path) };
	if (surface == nullptr) {
		PrintLine(IMG_GetError());
		assert(!"Failed to create texture from image path");
	}
	texture_ = std::shared_ptr<SDL_Texture>(SDL_CreateTextureFromSurface(global::GetGame().systems.sdl.GetRenderer(), surface), SDL_DestroyTexture);
	if (!IsValid()) {
		PrintLine(SDL_GetError());
		assert(!"Failed to create texture");
	}
	SDL_FreeSurface(surface);
}


Texture::Texture(SDL_Surface* surface) {
	assert(surface != nullptr && "Nullptr surface?");
	texture_ = std::shared_ptr<SDL_Texture>(SDL_CreateTextureFromSurface(global::GetGame().systems.sdl.GetRenderer(), surface), SDL_DestroyTexture);
	if (!IsValid()) {
		PrintLine(SDL_GetError());
		assert(!"Failed to create texture from surface");
	}
	SDL_FreeSurface(surface);
}

bool Texture::IsValid() const {
	return texture_ != nullptr;
}

void Texture::Draw(const Rectangle<int>& texture,
				   const Rectangle<int>& source) const {
	auto renderer{ global::GetGame().systems.sdl.GetRenderer() };
	assert(renderer != nullptr && "Game uninitialized?");
	assert(IsValid() && "Destroyed or uninitialized texture?");
	SDL_Rect* src{ NULL };
	SDL_Rect src_rect;
	if (!source.size.IsZero()) {
		src_rect = { source.pos.x,  source.pos.y,
			         source.size.x, source.size.y };
		src = &src_rect;
	}
	SDL_Rect destination{ texture.pos.x,  texture.pos.y,
		                  texture.size.x, texture.size.y };
	SDL_RenderCopy(renderer, texture_.get(), src, &destination);
}

} // namespace ptgn
