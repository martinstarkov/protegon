#pragma once

#include "math/Vector2.h"
#include "texture/Flip.h"

struct SDL_Texture;
struct SDL_Surface;

namespace ptgn {

namespace internal {

class Texture {
public:
	Texture() = default;
	Texture(const char* texture_path);
	Texture(SDL_Surface* surface);
	~Texture();
	void Reset(SDL_Surface* surface);
	operator SDL_Texture*() const;
private:
	void Set(SDL_Surface* surface);
	SDL_Texture* texture_{ nullptr };
};

} // namespace internal

} // namespace ptgn