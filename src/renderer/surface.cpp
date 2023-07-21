#include "protegon/surface.h"

#include <SDL.h>
#include <SDL_image.h>

#include <cassert> // assert

#include "protegon/log.h"
#include "utility/file.h"

namespace ptgn {

Surface::Surface(const char* image_path) {
	assert(*image_path && "Empty image path?");
	//assert(FileExists(image_path) && "Nonexistent image path?");
	auto surface{ IMG_Load(image_path) };
	if (surface == nullptr) {
		PrintLine(IMG_GetError());
		assert(!"Failed to create surface from image path");
	}
	surface_ = std::shared_ptr<SDL_Surface>(surface, SDL_FreeSurface);
	if (!IsValid()) {
		PrintLine(SDL_GetError());
		assert(!"Failed to create surface");
	}
}

void Surface::Lock() {
	int success = SDL_LockSurface(surface_.get());
	if (success != 0) {
		PrintLine(SDL_GetError());
		assert(!"Failed to lock surface");
	}
}

V2_int Surface::GetSize() const {
	return { surface_.get()->w, surface_.get()->h };
}

void Surface::Unlock() {
	SDL_UnlockSurface(surface_.get());
}

Color Surface::GetColor(const V2_int& coordinate) {
	return GetColorData(GetPixelData(coordinate));
}

Color Surface::GetColorData(std::uint32_t pixel_data) {
	Color color;
	SDL_GetRGB(pixel_data, surface_.get()->format, &color.r, &color.g, &color.b);
	return color;
}

std::uint32_t Surface::GetPixelData(const V2_int& coordinate) {
	int bpp = surface_.get()->format->BytesPerPixel;
	std::uint8_t* p = (std::uint8_t*)surface_.get()->pixels + coordinate.y * surface_.get()->pitch + coordinate.x * bpp;
	assert(p != nullptr && "Failed to find coordinate pixel data");
	switch (bpp) {
		case 1:
			return *p;
			break;
		case 2:
			return *(std::uint16_t*)p;
			break;

		case 3:
			if (SDL_BYTEORDER == SDL_BIG_ENDIAN)
				return p[0] << 16 | p[1] << 8 | p[2];
			else
				return p[0] | p[1] << 8 | p[2] << 16;
			break;

		case 4:
			return *(uint32_t*)p;
			break;

		default:
			return 0;       /* shouldn't happen, but avoids warnings */
	}
}

bool Surface::IsValid() const {
	return surface_ != nullptr;
}

} // namespace ptgn
