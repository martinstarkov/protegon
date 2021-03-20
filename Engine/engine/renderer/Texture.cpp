#include "Texture.h"

#include <SDL.h>

#include "renderer/Color.h"

namespace engine {

Texture::Texture(SDL_Texture* texture) : texture{ texture } {}
Texture::Texture(const Renderer& renderer, PixelFormat format, TextureAccess texture_access, const V2_int& size) : texture{ SDL_CreateTexture(renderer, static_cast<std::uint32_t>(format), static_cast<int>(texture_access), size.x, size.y) } {
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
bool Texture::Lock(void** out_pixels, int* out_pitch, AABB* lock_area) {
	SDL_Rect* lock_rect{ NULL };
	SDL_Rect rect;
	if (lock_area != nullptr) {
		rect.x = math::Ceil(lock_area->position.x);
		rect.y = math::Ceil(lock_area->position.y);
		rect.w = math::Ceil(lock_area->size.x);
		rect.h = math::Ceil(lock_area->size.y);
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

void Texture::Clear() {
	void* pixels;
	int pitch;
	Lock(&pixels, &pitch);
	int w, h;
	SDL_QueryTexture(texture, NULL, NULL, &w, &h);
	for (auto y{ 0 }; y < h; ++y) {
		auto dst{ (std::uint32_t*)((std::uint8_t*)pixels + y * pitch) };
		for (auto x{ 0 }; x < w; ++x) {
			*dst++ = (0xFF000000 | (0 << 16) | (0 << 8) | 0); // Set texture to black.
		}
	}
	Unlock();
}

} // namespace engine