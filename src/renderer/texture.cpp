#include "protegon/texture.h"

#include <SDL.h>
#include <SDL_image.h>

#include "utility/debug.h"
#include "core/game.h"

namespace ptgn {

Texture::Texture(const path& image_path) {
	PTGN_CHECK(FileExists(image_path), "Cannot create texture from a nonexistent image path");
	instance_ = {
		IMG_LoadTexture(
			global::GetGame().sdl.GetRenderer().get(), 
			image_path.string().c_str()
		),
		SDL_DestroyTexture
	};
	if (!IsValid()) {
		PTGN_ERROR(IMG_GetError());
		PTGN_ASSERT(false, "Failed to create texture from image path");
	}
}

Texture::Texture(AccessType access, const V2_int& size) {
	instance_ = { SDL_CreateTexture(global::GetGame().sdl.GetRenderer().get(), SDL_PIXELFORMAT_RGBA8888,
				  static_cast<SDL_TextureAccess>(access), size.x, size.y), SDL_DestroyTexture };
	PTGN_ASSERT(IsValid(), "Failed to create texture from access type and size");
}

Texture::Texture(const std::shared_ptr<SDL_Surface>& surface) {
	PTGN_ASSERT(surface != nullptr, "Cannot create texture from uninitialized or destroyed surface");
	instance_ = {
		SDL_CreateTextureFromSurface(
			global::GetGame().sdl.GetRenderer().get(),
			surface.get()
		),
		SDL_DestroyTexture
	};
	if (!IsValid()) {
		PTGN_ERROR(SDL_GetError());
		PTGN_ASSERT(false, "Failed to create texture from surface");
	}
}

void Texture::Draw(const Rectangle<float>& texture,
				   const Rectangle<int>& source,
				   float angle,
				   Flip flip,
				   V2_int* center_of_rotation) const {
	PTGN_CHECK(IsValid(), "Cannot draw uninitialized or destroyed texture");
	
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
	PTGN_CHECK(IsValid(), "Cannot get size of uninitialized or destroyed texture");
	V2_int size;
	SDL_QueryTexture(instance_.get(), NULL, NULL, &size.x, &size.y);
	return size;
}

void Texture::SetBlendMode(BlendMode mode) {
	PTGN_CHECK(IsValid(), "Cannot set blend mode of uninitialized or destroyed texture");
	SDL_SetTextureBlendMode(instance_.get(), static_cast<SDL_BlendMode>(mode));
}

void Texture::SetAlpha(std::uint8_t alpha) {
	PTGN_CHECK(IsValid(), "Cannot set alpha of uninitialized or destroyed texture");
	SDL_SetTextureBlendMode(instance_.get(), static_cast<SDL_BlendMode>(BlendMode::BLEND));
	SDL_SetTextureAlphaMod(instance_.get(), alpha);
}

void Texture::SetColor(const Color& color) {
	PTGN_CHECK(IsValid(), "Cannot set color of uninitialized or destroyed texture");
	SetAlpha(color.a);
	SDL_SetTextureColorMod(instance_.get(), color.r, color.g, color.b);
}

Texture::AccessType Texture::GetAccessType() const {
	PTGN_CHECK(IsValid(), "Cannot get access type of uninitialized or destroyed texture");
	int access;
	SDL_QueryTexture(instance_.get(), nullptr, &access, NULL, NULL);
	return static_cast<Texture::AccessType>(access);
}

void Texture::SetAsRendererTarget() {
	PTGN_CHECK(IsValid(), "Cannot set uninitialized or destroyed texture as renderer target");
	PTGN_CHECK(GetAccessType() == AccessType::TARGET, "Cannot set texture as renderer target if it was not created with target access type flag");
	SDL_SetRenderTarget(global::GetGame().sdl.GetRenderer().get(), instance_.get());
}

} // namespace ptgn