#include "PixelFormat.h"

#include <SDL.h>

namespace ptgn {

PixelFormat::PixelFormat(SDL_PixelFormat* format) : format_{ format } {}

PixelFormat::operator SDL_PixelFormat* () const {
	return format_;
}

SDL_PixelFormat* PixelFormat::operator&() const {
	return format_;
}

} // namespace ptgn