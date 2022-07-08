#include "PixelFormat.h"

#include <SDL.h>

namespace ptgn {

void PixelFormat::Destroy() {
	SDL_FreeFormat(format_);
}

} // namespace ptgn