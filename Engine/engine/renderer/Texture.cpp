#include "Texture.h"

#include <SDL.h>

#include "renderer/AABB.h"

namespace engine {

Texture::Texture(SDL_Texture* texture) : texture{ texture } {}
SDL_Texture* Texture::operator=(SDL_Texture* texture) { this->texture = texture; return this->texture; }
Texture::operator SDL_Texture* () const { return texture; }
Texture::operator bool() const { return texture != nullptr; }
SDL_Texture* Texture::operator&() const { return texture; }
bool Texture::Lock(void* out_pixels, int* out_pitch, AABB* lock_area) {
	SDL_Rect* lock_rect = NULL;
	SDL_Rect rect;
	if (lock_area != nullptr) {
		rect.x = lock_area->position.x;
		rect.y = lock_area->position.y;
		rect.w = lock_area->size.x;
		rect.h = lock_area->size.y;
		lock_rect = &rect;
	}
	if (SDL_LockTexture(texture, lock_rect, &out_pixels, out_pitch) < 0) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't lock texture: %s\n", SDL_GetError());
		return false;
	}
	return true;
}

void Texture::Unlock() {
	SDL_UnlockTexture(texture);
}

} // namespace engine