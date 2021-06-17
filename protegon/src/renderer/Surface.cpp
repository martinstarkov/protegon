#include "Surface.h"

#include <SDL.h>
#include <SDL_image.h>

#include "debugging/Debug.h"

namespace ptgn {

const std::uint32_t* Surface::GetPixel(const V2_int& position) const {
    int bytes_per_pixel = surface_->format->BytesPerPixel;
	assert(bytes_per_pixel == 1 ||
		   bytes_per_pixel == 2 ||
		   bytes_per_pixel == 4 &&
		   "Invalid bytes per pixel for surface pixel retrieval");
    std::uint8_t* p = 
		(std::uint8_t*)surface_->pixels +
		position.y * surface_->pitch + 
		position.x * bytes_per_pixel;
	assert(position.x < surface_->w &&
		   "Cannot retrieve surface pixel for x position greater than surface width");
	assert(position.x >= 0 &&
		   "Cannot retrieve surface pixel for x position smaller than 0");
	assert(position.y < surface_->h &&
		   "Cannot retrieve surface pixel for y position greater than surface height");
	assert(position.y >= 0 &&
		   "Cannot retrieve surface pixel for y position smaller than 0");
    return (std::uint32_t*)p;
}

std::uint32_t* Surface::GetPixel(const V2_int& position) {
	return const_cast<std::uint32_t*>(static_cast<const Surface&>(*this).GetPixel(position));
}

void* const Surface::GetPixels() const {
	return surface_->pixels;
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

Surface::Surface(SDL_Surface* surface) : surface_{ surface } {}

Surface::Surface(const char* img_file_path) : 
	surface_{ IMG_Load(img_file_path) } {
	if (!IsValid()) {
		PrintLine("Failed to create surface from image: ", IMG_GetError());
		abort();
	}
}

Surface::operator SDL_Surface*() const {
	return surface_;
}

SDL_Surface* Surface::operator&() const {
	return surface_;
}

bool Surface::IsValid() const {
	return surface_ != nullptr;
}

void Surface::Destroy() {
	SDL_FreeSurface(surface_);
	surface_ = nullptr;
}

} // namespace ptgn