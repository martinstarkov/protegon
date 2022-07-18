#pragma once

#include "math/Vector2.h"
#include "renderer/Flip.h"

struct SDL_Texture;
struct SDL_Surface;

namespace ptgn {

class Texture {
public:
	Texture() = default;
	Texture(const char* texture_path);
	Texture(SDL_Surface* surface) { Set(surface); }
	~Texture();
	void Reset(SDL_Surface* surface);
	bool Exists() const { return texture_ != nullptr; }
	operator SDL_Texture*() const {
		assert(Exists() && "Cannot cast nullptr texture to SDL_Texture");
		return texture_;
	}
private:
	void Set(SDL_Surface* surface);
	SDL_Texture* texture_{ nullptr };
};

} // namespace ptgn