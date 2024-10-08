#pragma once

namespace ptgn {

enum class Mouse {
	Left   = 1, // SDL_BUTTON_LEFT
	Middle = 2, // SDL_BUTTON_MIDDLE
	Right  = 3	// SDL_BUTTON_RIGHT
};

// Enum for storing states of mouse keys.
enum class MouseState {
	Up		 = 1,
	Down	 = 2,
	Released = 3,
	Pressed	 = 4
};

} // namespace ptgn