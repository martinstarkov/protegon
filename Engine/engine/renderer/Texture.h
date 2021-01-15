#pragma once

struct SDL_Texture;

struct AABB;

namespace engine {

struct Texture {
	Texture() = default;
	Texture(SDL_Texture* texture);
	SDL_Texture* operator=(SDL_Texture* texture);
	operator SDL_Texture* () const;
	operator bool() const;
	bool Lock(void** out_pixels, int* out_pitch, AABB* lock_area = nullptr);
	void Unlock();
	void Clear();
	SDL_Texture* operator&() const;
	SDL_Texture* texture = nullptr;
};

} // namespace engine