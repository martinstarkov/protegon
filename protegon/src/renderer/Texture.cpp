#include "Texture.h"

#include <SDL.h>

#include "debugging/Debug.h"
#include "renderer/Renderer.h"
#include "renderer/Surface.h"

namespace engine {

Texture::Texture(SDL_Texture* texture) : texture_{ texture } {}

Texture::Texture(const Renderer& renderer, 
				 const V2_int& size, 
				 PixelFormat format, 
				 TextureAccess texture_access) {
	assert(renderer.IsValid() && "Cannot create texture from invalid renderer");
	texture_ = SDL_CreateTexture(renderer,
								 static_cast<std::uint32_t>(format),
								 static_cast<int>(texture_access),
								 size.x,
								 size.y);
	if (!IsValid()) {
		PrintLine("Failed to create texture: ", SDL_GetError());
		abort();
	}
}

Texture::Texture(const Renderer& renderer, 
				 const internal::Surface& surface) {
	assert(renderer.IsValid() && "Cannot create texture from invalid renderer");
	assert(surface.IsValid() && "Cannot create texture from invalid surface");
	texture_ = SDL_CreateTextureFromSurface(renderer, surface);
	if (!IsValid()) {
		PrintLine("Failed to create texture: ", SDL_GetError());
		abort();
	}
}

SDL_Texture* Texture::operator=(SDL_Texture* texture) {
	this->texture_ = texture;
	return this->texture_;
}

Texture::operator SDL_Texture*() const {
	return texture_;
}

bool Texture::IsValid() const {
	return texture_ != nullptr;
}

SDL_Texture* Texture::operator&() const {
	return texture_;
}

bool Texture::Lock(void** out_pixels, 
				   int* out_pitch, 
				   V2_int lock_position, 
				   V2_int lock_size) {
	SDL_Rect* lock_rect{ NULL };
	SDL_Rect rect;
	if (!lock_size.IsZero()) {
		rect = { lock_position.x, lock_position.y, lock_size.x, lock_size.y };
		lock_rect = &rect;
	}
	if (SDL_LockTexture(texture_, lock_rect, out_pixels, out_pitch) < 0) {
		PrintLine("Couldn't lock texture: ", SDL_GetError());
		return false;
	}
	return true;
}

void Texture::Unlock() {
	if (texture_ != nullptr) {
		SDL_UnlockTexture(texture_);
	}
}

void Texture::Destroy() {
	if (texture_ != nullptr) {
		SDL_DestroyTexture(texture_);
		texture_ = nullptr;
	}
}

void Texture::SetColor(const Color& color, 
					   PixelFormat format) {
	void* pixels{ nullptr };
	int pitch{ 0 };
	Lock(&pixels, &pitch);
	V2_int size;
	SDL_QueryTexture(texture_, NULL, NULL, &size.x, &size.y);
	auto pixel_color{ color.ToUint32(format) };
	for (auto y{ 0 }; y < size.y; ++y) {
		auto dst{ (std::uint32_t*)((std::uint8_t*)pixels + y * pitch) };
		for (auto x{ 0 }; x < size.x; ++x) {
			// Set all texture pixels to black.
			*dst++ = pixel_color;
		}
	}
	Unlock();
}

V2_int Texture::GetSize() const {
	V2_int size;
	auto output{ SDL_QueryTexture(texture_,
								  NULL, NULL,
								  &size.x, &size.y)
	};
	assert(output == 0 && "Cannot query invalid texture for size");
	return size;
}

TextureAccess Texture::GetTextureAccess() const {
	int access{ 0 };
	auto output{ SDL_QueryTexture(texture_,
								  NULL, &access,
								  NULL, NULL)
	};
	assert(output == 0 && "Cannot query invalid texture for texture access");
	return static_cast<TextureAccess>(access);
}

PixelFormat Texture::GetPixelFormat() const {
	std::uint32_t format{ 0 };
	auto output{ SDL_QueryTexture(texture_,
								  &format, NULL,
								  NULL, NULL)
	};
	assert(output == 0 && "Cannot query invalid texture for pixel format");
	return static_cast<PixelFormat>(format);
}

} // namespace engine