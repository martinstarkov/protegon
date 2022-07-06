#pragma once

#include "math/Vector2.h"
#include "renderer/Flip.h"

struct SDL_Texture;
struct SDL_Surface;
struct SDL_Renderer;

namespace ptgn {

class Texture {
public:
	Texture() = default;
	Texture(SDL_Renderer* renderer, const char* texture_path);
	Texture(SDL_Renderer* renderer, SDL_Surface* surface);
	~Texture();
	void Reset(SDL_Renderer* renderer, SDL_Surface* surface);
	operator SDL_Texture*() const;
private:
	void Set(SDL_Renderer* renderer, SDL_Surface* surface);
	SDL_Texture* texture_{ nullptr };
};

} // namespace ptgn