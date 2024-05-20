#include "protegon/texture.h"

#include <SDL.h>
#include <SDL_image.h>

#include <cassert> // assert

#include "protegon/log.h"
#include "protegon/file.h"
#include "core/game.h"

namespace ptgn {

Texture::Texture(const char* image_path) {
	assert(*image_path && "Empty image path?");
	assert(FileExists(image_path) && "Nonexistent image path?");
	auto surface{ IMG_Load(image_path) };
	if (surface == nullptr) {
		PrintLine(IMG_GetError());
		assert(!"Failed to create texture from image path");
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

void Texture::Draw(Rectangle<float> texture,
				   const Rectangle<int>& source,
				   float angle,
				   Flip flip,
				   V2_int* center_of_rotation) const {
	auto renderer{ global::GetGame().sdl.GetRenderer() };
	assert(renderer != nullptr && "Game uninitialized?");
	assert(IsValid() && "Destroyed or uninitialized texture?");
	SDL_Rect* src{ NULL };
	SDL_Rect src_rect;
	if (!source.size.IsZero()) {
		src_rect = { source.pos.x,  source.pos.y,
			         source.size.x, source.size.y };
		src = &src_rect;
	}
	V2_float scale = window::GetScale();
	texture.pos *= scale;
	texture.size *= scale;
	SDL_Rect destination{
		static_cast<int>(texture.pos.x),
		static_cast<int>(texture.pos.y),
		static_cast<int>(texture.size.x),
		static_cast<int>(texture.size.y)
	};
	SDL_Point rotation_point;
	if (center_of_rotation != nullptr) {
		rotation_point.x = center_of_rotation->x;
		rotation_point.y = center_of_rotation->y;
	}
	SDL_RenderCopyEx(renderer,
		texture_.get(),
		src,
		&destination,
		angle,
		center_of_rotation != nullptr ? &rotation_point : NULL,
		static_cast<SDL_RendererFlip>(flip)
	);
}

V2_int Texture::GetSize() const {
	V2_int size;
	SDL_QueryTexture(texture_.get(), NULL, NULL, &size.x, &size.y);
	return size;
}

void Texture::SetAlpha(std::uint8_t alpha) {
	SDL_SetTextureBlendMode(texture_.get(), SDL_BLENDMODE_BLEND);
	SDL_SetTextureAlphaMod(texture_.get(), alpha);
}

void Texture::SetColor(const Color& color) {
	SetAlpha(color.a);
	SDL_SetTextureColorMod(texture_.get(), color.r, color.g, color.b);
}

} // namespace ptgn
