#pragma once

struct SDL_Texture;

namespace engine {

struct Texture {
	Texture() = default;
	Texture(SDL_Texture* texture);
	SDL_Texture* operator=(SDL_Texture* texture);
	operator SDL_Texture* () const;
	operator bool() const;
	SDL_Texture* operator&() const;
	SDL_Texture* texture = nullptr;
};

} // namespace engine