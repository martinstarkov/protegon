#pragma once

#include <cstdint> // std::uint8_t

#include "math/Vector2.h"
#include "renderer/PixelFormat.h"
#include "renderer/Color.h"

struct SDL_Surface;

namespace ptgn {

class Text;
class Texture;
class TextureManager;

class Surface {
public:
	// Returns the color data at a given position on a surface.
	Color GetPixel(const V2_int& position) const;

	int GetPitch() const;

	V2_int GetSize() const;

	std::uint8_t GetBytesPerPixel() const;

	PixelFormat GetPixelFormat() const;
private:
	friend class Level;
	friend class Text;
	friend class Texture;
	friend class TextureManager;

	std::uint32_t GetPixelData(const V2_int& position) const;

	Surface() = default;

	// Create a surface from an image file path.
	Surface(const char* img_file_path);

	// Conversion operators.

	operator SDL_Surface* () const;
	SDL_Surface* operator&() const;

	/*
	* @return True if SDL_Surface is not nullptr, false otherwise.
	*/
	bool IsValid() const;

	/*
	* Frees memory used by SDL_Surface.
	*/
	void Destroy();

	Surface(SDL_Surface* surface);

	SDL_Surface* surface_{ nullptr };
};

} // namespace ptgn