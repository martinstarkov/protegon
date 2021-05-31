#include "Surface.h"

#include <SDL.h>
#include <SDL_image.h>

#include "debugging/Debug.h"

namespace ptgn {

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