#pragma once
#include <iosfwd>

#include "debug/log.h"

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

inline std::ostream& operator<<(std::ostream& os, Mouse mouse) {
	switch (mouse) {
		case Mouse::Left:	os << "Left"; break;
		case Mouse::Right:	os << "Right"; break;
		case Mouse::Middle: os << "Middle"; break;
		default:			PTGN_ERROR("Invalid mouse type");
	}

	return os;
}

} // namespace ptgn