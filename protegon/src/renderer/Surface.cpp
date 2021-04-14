#include "Surface.h"

#include <SDL.h>
#include <SDL_ttf.h>

namespace engine {

Surface::Surface(SDL_Surface* surface) : surface_{ surface } {}

Surface::operator SDL_Surface* () const {
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

} // namespace engine