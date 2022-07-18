#pragma once

#include <cstdint> // std::uint32_t

struct SDL_PixelFormat;

namespace ptgn {

class PixelFormat {
public:

private:
	friend class Surface;
	friend class Texture;
	friend class Color;
	friend class TileMap;

	PixelFormat(SDL_PixelFormat* format) : format_{ format } {}

	// Frees memory used by format pointer.
	void Destroy();

	operator SDL_PixelFormat* () const { return format_; }
	SDL_PixelFormat* operator&() const { return format_; }
	
	SDL_PixelFormat* format_{ nullptr };
};

} // namespace ptgn