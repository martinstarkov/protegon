#include "protegon/texture.h"

#include <SDL.h>
#include <SDL_image.h>

#include <cassert> // assert

#include "protegon/log.h"
#include "core/game.h"
#include "utility/file.h"

namespace ptgn {

Texture::Texture(const char* texture_path) {
	assert(texture_path != "" && "Empty path?");
	assert(FileExists(texture_path) && "Nonexistent file path?");
	auto surface{ IMG_Load(texture_path) };
	if (surface == nullptr) {
		PrintLine(IMG_GetError());
		assert(!"Failed to create texture from texture path");
	}
	texture_ = std::shared_ptr<SDL_Texture>(SDL_CreateTextureFromSurface(global::GetGame().sdl.GetRenderer(), surface), SDL_DestroyTexture);
	if (!IsValid()) {
		PrintLine(SDL_GetError());
		assert(!"Failed to create texture");
	}
	SDL_FreeSurface(surface);
}

Texture::Texture(SDL_Surface* surface) {
	assert(surface != nullptr && "Nullptr surface?");
	texture_ = std::shared_ptr<SDL_Texture>(SDL_CreateTextureFromSurface(global::GetGame().sdl.GetRenderer(), surface), SDL_DestroyTexture);
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
	auto renderer{ global::GetGame().sdl.GetRenderer() };
	assert(renderer != nullptr && "Game uninitialized?");
	assert(IsValid() && "Destroyed or uninitialized texture?");
	SDL_Rect* src{ NULL };
	SDL_Rect src_rect;
	if (!source.size.IsZero()) {
		src_rect = { source.position.x, source.position.y,
			         source.size.x,     source.size.y };
		src = &src_rect;
	}
	SDL_Rect destination{ texture.position.x, texture.position.y,
		                  texture.size.x,     texture.size.y };
	SDL_RenderCopy(renderer, texture_.get(), src, &destination);
}

} // namespace ptgn