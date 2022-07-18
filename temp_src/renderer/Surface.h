#pragma once

#include <cstdint> // std::uint8_t

#include "math/Vector2.h"
#include "renderer/PixelFormat.h"
#include "renderer/Color.h"

struct SDL_Surface;

namespace ptgn {

class Surface {
public:
	Surface() = default;
	// Create a surface from an image file path.
	Surface(const char* img_file_path);
	Surface::Surface(SDL_Surface* surface) : surface_{ surface } {}
	/*
	* Frees memory used by SDL_Surface.
	*/
	~Surface();
	/*
	* @return True if SDL_Surface is not nullptr, false otherwise.
	*/
	bool IsValid() const { return surface_ != nullptr; }
	// Returns the color data at a given position on a surface.
	Color GetPixel(const V2_int& position) const;
	int GetPitch() const;
	V2_int GetSize() const;
	std::uint8_t GetBytesPerPixel() const;
	PixelFormat GetPixelFormat() const;
	std::uint32_t GetPixelData(const V2_int& position) const;
	operator SDL_Surface* () const { return surface_; }
private:
	SDL_Surface* surface_{ nullptr };
};

} // namespace ptgn