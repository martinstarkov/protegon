#include "protegon/surface.h"

#include <SDL.h>
#include <SDL_image.h>

#include "protegon/debug.h"
#include "protegon/file.h"

namespace ptgn {

Surface::Surface(const path& image_path) {
	PTGN_CHECK(FileExists(image_path), "Cannot create surface from a nonexistent image path");
	instance_ = { IMG_Load(image_path.string().c_str()), SDL_FreeSurface };
	if (!IsValid()) {
		PTGN_ERROR(IMG_GetError());
		PTGN_ASSERT(false, "Failed to create surface from image path");
	}
}

void Surface::ForEachPixel(std::function<void(const V2_int&, const Color&)> function) {
	PTGN_CHECK(IsValid(), "Cannot loop through each pixel of uninitialized or destroyed surface");
	Lock();

	const V2_int size{ GetSize() };

	for (int i = 0; i < size.x; i++)
		for (int j = 0; j < size.y; j++) {
			V2_int coordinate{ i, j };
			function(coordinate, GetColor(coordinate));
		}

	Unlock();
}

void Surface::Lock() {
	PTGN_CHECK(IsValid(), "Cannot lock an uninitialized or destroyed surface");
	int success = SDL_LockSurface(instance_.get());
	if (success != 0) {
		PTGN_ERROR(SDL_GetError());
		PTGN_ASSERT(false, "Failed to lock surface");
	}
}

V2_int Surface::GetSize() const {
	PTGN_CHECK(IsValid(), "Cannot get size of an uninitialized or destroyed surface");
	return { instance_.get()->w, instance_.get()->h };
}

void Surface::Unlock() {
	PTGN_CHECK(IsValid(), "Cannot unlock an uninitialized or destroyed surface");
	SDL_UnlockSurface(instance_.get());
}

Color Surface::GetColor(const V2_int& coordinate) {
	PTGN_CHECK(IsValid(), "Cannot get color of an uninitialized or destroyed surface");
	return GetColorData(GetPixelData(coordinate));
}

Color Surface::GetColorData(std::uint32_t pixel_data) {
	PTGN_CHECK(IsValid(), "Cannot get color data of an uninitialized or destroyed surface");
	Color color;
	SDL_GetRGB(pixel_data, instance_.get()->format, &color.r, &color.g, &color.b);
	return color;
}

std::uint32_t Surface::GetPixelData(const V2_int& coordinate) {
	PTGN_CHECK(IsValid(), "Cannot get pixel data of an uninitialized or destroyed surface");
	int bpp = instance_.get()->format->BytesPerPixel;
	std::uint8_t* p = (std::uint8_t*)instance_.get()->pixels + 
					  coordinate.y * instance_.get()->pitch + coordinate.x * bpp;
	PTGN_CHECK(p != nullptr, "Failed to find coordinate pixel data");
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
			PTGN_ASSERT(false, "Failed to find coordinate pixel data");
			return 0;
	}
}

} // namespace ptgn
