#include "Surface.h"

#include <SDL.h>
#include <SDL_ttf.h>

namespace engine {

Surface::Surface(SDL_Surface* surface) : surface{ surface } {}

Surface::operator SDL_Surface* () const {
	return surface;
}

bool Surface::IsValid() const {
	return surface != nullptr;
}

SDL_Surface* Surface::operator&() const {
	return surface;
}

void Surface::Destroy() {
	SDL_FreeSurface(surface);
	surface = nullptr;
}

} // namespace engine