#include "Texture.h"

#include <SDL.h>

#include "renderer/Renderer.h"

#include "debugging/Debug.h"

namespace engine {

Texture::Texture(SDL_Texture* texture) : texture_{ texture } {}

Texture::Texture(const Renderer& renderer, const V2_int& size, PixelFormat pixel_format, TextureAccess texture_access) : texture_{ SDL_CreateTexture(renderer, static_cast<std::uint32_t>(pixel_format), static_cast<int>(texture_access), size.x, size.y) } {
	if (!IsValid()) {
		PrintLine("Failed to create texture: ", SDL_GetError());
		abort();
	}
}

Texture::Texture(const Renderer& renderer, const Surface& surface) {
	assert(renderer.IsValid() && "Cannot create texture from invalid renderer");
	assert(surface.IsValid() && "Cannot create texture from invalid surface");
	texture_ = SDL_CreateTextureFromSurface(renderer, surface);
	if (!IsValid()) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create texture: %s\n", SDL_GetError());
		assert(!true);
	}
}

SDL_Texture* Texture::operator=(SDL_Texture* texture) {
	this->texture_ = texture;
	return this->texture_;
}

Texture::operator SDL_Texture* () const {
	return texture_;
}

bool Texture::IsValid() const {
	return texture_ != nullptr;
}

SDL_Texture* Texture::operator&() const {
	return texture_;
}

bool Texture::Lock(void** out_pixels, int* out_pitch, V2_int lock_position, V2_int lock_size) {
	SDL_Rect* lock_rect{ NULL };
	SDL_Rect rect;
	if (!lock_size.IsZero()) {
		rect = { lock_position.x, lock_position.y, lock_size.x, lock_size.y };
		lock_rect = &rect;
	}
	if (SDL_LockTexture(texture_, lock_rect, out_pixels, out_pitch) < 0) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't lock texture: %s\n", SDL_GetError());
		return false;
	}
	return true;
}

void Texture::Unlock() {
	SDL_UnlockTexture(texture_);
}

void Texture::Destroy() {
	SDL_DestroyTexture(texture_);
	texture_ = nullptr;
}

void Texture::SetColor(const Color& color, PixelFormat pixel_format) {
	void* pixels{ nullptr };
	int pitch{ 0 };
	Lock(&pixels, &pitch);
	int width{ 0 };
	int height{ 0 };
	SDL_QueryTexture(texture_, NULL, NULL, &width, &height);
	auto pixel_color{ color.ToUint32(pixel_format) };
	for (auto y{ 0 }; y < height; ++y) {
		auto dst{ (std::uint32_t*)((std::uint8_t*)pixels + y * pitch) };
		for (auto x{ 0 }; x < width; ++x) {
			// Set all texture pixels to black.
			*dst++ = pixel_color;
		}
	}
	Unlock();
}

void Texture::Clear(PixelFormat pixel_format) {
	SetColor(colors::BLACK, pixel_format);
}

} // namespace engine