#pragma once

#include "rectangle.h"
#include "vector2.h"

struct SDL_Texture;
struct SDL_Surface;

namespace ptgn {

class Texture {
public:
	Texture() = delete;
	Texture(const char* texture_path);
	~Texture();
	bool IsValid() const;
	void Draw(const Rectangle<int>& texture,
			  const Rectangle<int>& source = {}) const;
private:
	friend class Text;
	// Takes ownership of surface pointer.
	Texture(SDL_Surface* surface);
	SDL_Texture* texture_{ nullptr };
};

} // namespace ptgn