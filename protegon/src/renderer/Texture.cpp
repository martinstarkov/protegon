#include "Texture.h"

#include <SDL.h>

#include "renderer/Color.h"

namespace engine {

Texture::~Texture() {}
Texture::Texture(SDL_Texture* texture) : texture{ texture } {}
Texture::Texture(const Renderer& renderer, const V2_int& size, PixelFormat format, TextureAccess texture_access) : texture{ SDL_CreateTexture(renderer, static_cast<std::uint32_t>(format), static_cast<int>(texture_access), size.x, size.y) } {
	if (!IsValid()) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create texture: %s\n", SDL_GetError());
		assert(!true);
	}
}
Texture::Texture(const Renderer& renderer, SDL_Surface* surface) : texture{ SDL_CreateTextureFromSurface(renderer, surface) } {
	if (!IsValid()) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create texture: %s\n", SDL_GetError());
		assert(!true);
	}
}
SDL_Texture* Texture::operator=(SDL_Texture* texture) { this->texture = texture; return this->texture; }
Texture::operator SDL_Texture* () const { return texture; }
bool Texture::IsValid() const { return texture != nullptr; }
SDL_Texture* Texture::operator&() const { return texture; }

bool Texture::Lock(void** out_pixels, int* out_pitch, V2_int lock_position, V2_int lock_size) {
	SDL_Rect* lock_rect{ NULL };
	SDL_Rect rect;
	if (!lock_size.IsZero()) {
		rect = { lock_position.x, lock_position.y, lock_size.x, lock_size.y };
		lock_rect = &rect;
	}
	if (SDL_LockTexture(texture, lock_rect, out_pixels, out_pitch) < 0) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't lock texture: %s\n", SDL_GetError());
		return false;
	}
	return true;
}

void Texture::Unlock() {
	SDL_UnlockTexture(texture);
}

void Texture::Destroy() {
	SDL_DestroyTexture(texture);
}

void Texture::SetColor(const Color& color) {
	void* pixels{ nullptr };
	int pitch{ 0 };
	Lock(&pixels, &pitch);
	int width{ 0 };
	int height{ 0 };
	SDL_QueryTexture(texture, NULL, NULL, &width, &height);
	auto color_uint32_t{ color.ToUint32() };
	for (auto y{ 0 }; y < height; ++y) {
		auto dst{ (std::uint32_t*)((std::uint8_t*)pixels + y * pitch) };
		for (auto x{ 0 }; x < width; ++x) {
			// Set all texture pixels to black.
			*dst++ = color_uint32_t;
		}
	}
	Unlock();
}

void Texture::Clear() {
	SetColor(colors::BLACK);
}

} // namespace engine