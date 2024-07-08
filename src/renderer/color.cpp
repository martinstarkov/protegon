#include "protegon/color.h"

#include "SDL.h"

namespace ptgn {

Color::operator SDL_Color() const {
	return SDL_Color{ r, g, b, a };
}

} // namespace ptgn