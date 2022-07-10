#include "Surface.h"

#include <cassert> // assert

#include <SDL.h>
#include <SDL_image.h>

#include "utility/Log.h"

namespace ptgn {

Surface::Surface(const char* img_file_path) : surface_{ IMG_Load(img_file_path) } {
	if (!IsValid()) {
		PrintLine(IMG_GetError());
		assert(!"Failed to create surface from image");
	}
}

Surface::~Surface() {
	SDL_FreeSurface(surface_);
	surface_ = nullptr;
}

std::uint32_t Surface::GetPixelData(const V2_int& position) const {
	assert(position.x < surface_->w &&
		   "Cannot retrieve surface pixel for x position greater than surface width");
	assert(position.x >= 0 &&
		   "Cannot retrieve surface pixel for x position smaller than 0");
	assert(position.y < surface_->h &&
		   "Cannot retrieve surface pixel for y position greater than surface height");
	assert(position.y >= 0 &&
		   "Cannot retrieve surface pixel for y position smaller than 0");
	int bytes_per_pixel{ surface_->format->BytesPerPixel };
	std::uint8_t* pixel{ static_cast<std::uint8_t*>(surface_->pixels) + position.y * surface_->pitch + position.x * bytes_per_pixel };
	switch (bytes_per_pixel) {
		case 1:
			return *pixel;
		case 2:
			return *static_cast<std::uint16_t*>(static_cast<void*>(pixel));
		case 3:
			if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
				return pixel[0] << 16 | pixel[1] << 8 | pixel[2];
			} else {
				return pixel[0] | pixel[1] << 8 | pixel[2] << 16;
			}
		case 4:
			return *static_cast<std::uint32_t*>(static_cast<void*>(pixel));
		default:
			// Error
			return 0;
	}
}

Color Surface::GetPixel(const V2_int& position) const {
	return { GetPixelData(position), surface_->format };
}

int Surface::GetPitch() const {
	return surface_->pitch;
}

V2_int Surface::GetSize() const {
	return { surface_->w, surface_->h };
}

std::uint8_t Surface::GetBytesPerPixel() const {
	return surface_->format->BytesPerPixel;
}

PixelFormat Surface::GetPixelFormat() const {
	return surface_->format;
}

} // namespace ptgn