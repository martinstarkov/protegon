#pragma once

#include <cstdlib>
#include <cstdint>

namespace ptgn {

enum class Mouse : std::uint8_t {
	LEFT = 1,				// SDL_BUTTON_LEFT
	MIDDLE = 2,				// SDL_BUTTON_MIDDLE
	RIGHT = 3				// SDL_BUTTON_RIGHT
};

// Enum for storing states of mouse keys.
enum class MouseState : std::size_t {
	UP = 1,
	DOWN = 2,
	RELEASED = 3,
	PRESSED = 4
};

} // namespace ptgn