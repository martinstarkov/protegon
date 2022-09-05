#pragma once

#include <cstdlib> // std::size_t

namespace ptgn {

enum class Mouse : std::size_t {
	LEFT = 1,				// SDL_BUTTON_LEFT
	MIDDLE = 2,				// SDL_BUTTON_MIDDLE
	RIGHT = 3				// SDL_BUTTON_RIGHT
};

} // namespace ptgn