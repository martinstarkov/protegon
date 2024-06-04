#include "protegon/texture.h"

#include <SDL.h>
#include <SDL_image.h>

#include <cassert>

#include "protegon/log.h"
#include "core/game.h"

namespace ptgn {

Texture::Texture(const path& image_path) {
	assert(FileExists(image_path) && "Cannot create texture from a nonexistent image path");
	instance_ = {
		IMG_LoadTexture(
			global::GetGame().sdl.GetRenderer().get(), 
			image_path.string().c_str()
		),
		SDL_DestroyTexture
	};
	if (!IsValid()) {
		PrintLine(IMG_GetError());
		assert(!"Failed to create texture from image path");
	}
}

Texture::Texture(const std::shared_ptr<SDL_Surface>& surface) {
	assert(surface != nullptr && "Cannot create texture from uninitialized or destroyed surface");
	instance_ = {
		SDL_CreateTextureFromSurface(
			global::GetGame().sdl.GetRenderer().get(),
			surface.get()
		),
		SDL_DestroyTexture
	};
	if (!IsValid()) {
		PrintLine(SDL_GetError());
		assert(!"Failed to create texture from surface");
	}
}

void Texture::Draw(const Rectangle<float>& texture,
				   const Rectangle<int>& source,
				   float angle,
				   Flip flip,
				   V2_int* center_of_rotation) const {
	assert(IsValid() && "Cannot draw uninitialized or destroyed texture");
	
	SDL_Rect src_rect;
	bool source_given{ !source.size.IsZero() };
	if (source_given) {
		src_rect = { source.pos.x,  source.pos.y,
					 source.size.x, source.size.y };
	}
	
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

	SDL_RenderCopyEx(
		global::GetGame().sdl.GetRenderer().get(),
		instance_.get(),
		source_given ? &src_rect : NULL,
		&destination,
		angle,
		center_of_rotation != nullptr ? &rotation_point : NULL,
		static_cast<SDL_RendererFlip>(flip)
	);
}

V2_int Texture::GetSize() const {
	assert(IsValid() && "Cannot get size of uninitialized or destroyed texture");
	V2_int size;
	SDL_QueryTexture(instance_.get(), NULL, NULL, &size.x, &size.y);
	return size;
}

void Texture::SetAlpha(std::uint8_t alpha) {
	assert(IsValid() && "Cannot set alpha of uninitialized or destroyed texture");
	SDL_SetTextureBlendMode(instance_.get(), SDL_BLENDMODE_BLEND);
	SDL_SetTextureAlphaMod(instance_.get(), alpha);
}

void Texture::SetColor(const Color& color) {
	assert(IsValid() && "Cannot set color of uninitialized or destroyed texture");
	SetAlpha(color.a);
	SDL_SetTextureColorMod(instance_.get(), color.r, color.g, color.b);
}

} // namespace ptgn