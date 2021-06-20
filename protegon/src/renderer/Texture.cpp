#include "Texture.h"

#include <SDL.h>

#include "debugging/Debug.h"
#include "renderer/ScreenRenderer.h"
#include "renderer/Surface.h"

namespace ptgn {

Texture::Texture(SDL_Texture* texture) : texture_{ texture } {}

Texture::Texture(const ScreenRenderer& renderer, 
				 const V2_int& size, 
				 std::uint32_t format,
				 TextureAccess texture_access) {
	assert(renderer.IsValid() && "Cannot create texture from invalid renderer");
	texture_ = SDL_CreateTexture(renderer,
								 format,
								 static_cast<int>(texture_access),
								 size.x,
								 size.y);
	if (!IsValid()) {
		PrintLine("Failed to create texture: ", SDL_GetError());
		abort();
	}
}

Texture::Texture(const ScreenRenderer& renderer, const Surface& surface) {
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

void Texture::Lock(void** out_pixels, 
				   int* out_pitch, 
				   V2_int lock_position, 
				   V2_int lock_size) {
	SDL_Rect* lock_rect{ NULL };
	SDL_Rect rect;
	if (!lock_size.IsZero()) {
		rect = { lock_position.x, lock_position.y, lock_size.x, lock_size.y };
		lock_rect = &rect;
	}
	assert(IsValid() && "Cannot lock invalid texture");
	if (SDL_LockTexture(texture_, lock_rect, out_pixels, out_pitch) < 0) {
		PrintLine("Could not lock texture, ensure texture access is streaming: ", SDL_GetError());
		abort();
	}
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

void Texture::SetColor(const Color& color, PixelFormat format) {
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
								  &size.x, &size.y) };
	assert(output == 0 && "Cannot query invalid texture for size");
	return size;
}

TextureAccess Texture::GetTextureAccess() const {
	int access{ 0 };
	auto output{ SDL_QueryTexture(texture_,
								  NULL, &access,
								  NULL, NULL) };
	assert(output == 0 && "Cannot query invalid texture for texture access");
	return static_cast<TextureAccess>(access);
}

std::uint32_t Texture::GetPixelFormat() const {
	std::uint32_t format{ 0 };
	auto output{ SDL_QueryTexture(texture_,
								  &format, NULL,
								  NULL, NULL) };
	assert(output == 0 && "Cannot query invalid texture for pixel format");
	return format;
}

PixelFormat Texture::AllocatePixelFormat(std::uint32_t format) const {
	return SDL_AllocFormat(format);
}

void Texture::FreePixelFormat(PixelFormat format) const {
	SDL_FreeFormat(format);
}

int Texture::SlowGetBytesPerPixel() const {
	PixelFormat format = AllocatePixelFormat(GetPixelFormat());
	int bytes_per_pixel = format.format_->BytesPerPixel;
	FreePixelFormat(format);
	return bytes_per_pixel;
}

const std::uint32_t& Texture::GetPixel(void* pixels,
									   int pitch,
									   const V2_int& position,
									   int bytes_per_pixel) const {
	assert(position.x < GetSize().x &&
		   "Cannot retrieve texture pixel for x position greater than texture width");
	assert(position.x >= 0 &&
		   "Cannot retrieve texture pixel for x position smaller than 0");
	assert(position.y < GetSize().y &&
		   "Cannot retrieve texture pixel for y position greater than texture height");
	assert(position.y >= 0 &&
		   "Cannot retrieve texture pixel for y position smaller than 0");
	/* Here p is the address to the pixel we want to retrieve */
	std::uint8_t* p = (std::uint8_t*)pixels + position.y * pitch + position.x * bytes_per_pixel;
	switch (bytes_per_pixel) {
		case 1:
			return *p;
			break;
		case 2:
			return *(std::uint16_t*)p;
			break;
		case 3:
			if (SDL_BYTEORDER == SDL_BIG_ENDIAN)
				return p[0] << 16 | p[1] << 8 | p[2];
			else
				return p[0] | p[1] << 8 | p[2] << 16;
			break;
		case 4:
			return *(std::uint32_t*)p;
			break;
	}
	assert(bytes_per_pixel == 1 ||
		   bytes_per_pixel == 2 ||
		   bytes_per_pixel == 3 ||
		   bytes_per_pixel == 4 &&
		   "Invalid bytes per pixel for texture pixel retrieval");
	return *p;
}

std::uint32_t& Texture::GetPixel(void* pixels,
								 int pitch,
								 const V2_int& position,
								 int bytes_per_pixel) {
	return const_cast<std::uint32_t&>(static_cast<const Texture&>(*this).GetPixel(pixels, pitch, position, bytes_per_pixel));
}

} // namespace ptgn