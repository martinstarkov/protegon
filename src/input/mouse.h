#pragma once

#include <iosfwd>
#include <ostream>

#include "debug/log.h"
#include "serialization/enum.h"

namespace ptgn {

enum class Mouse {
	Invalid = -1,
	Left	= 1, // SDL_BUTTON_LEFT
	Middle	= 2, // SDL_BUTTON_MIDDLE
	Right	= 3	 // SDL_BUTTON_RIGHT
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
		case Mouse::Left:	 os << "Left"; break;
		case Mouse::Right:	 os << "Right"; break;
		case Mouse::Middle:	 os << "Middle"; break;
		case Mouse::Invalid: [[fallthrough]];
		default:			 PTGN_ERROR("Invalid mouse type")
	}

	return os;
}

PTGN_SERIALIZER_REGISTER_ENUM(
	Mouse, { { Mouse::Invalid, nullptr },
			 { Mouse::Left, "left" },
			 { Mouse::Middle, "middle" },
			 { Mouse::Right, "right" } }
);

PTGN_SERIALIZER_REGISTER_ENUM(
	MouseState, { { MouseState::Up, "up" },
				  { MouseState::Down, "down" },
				  { MouseState::Released, "released" },
				  { MouseState::Pressed, "pressed" } }
);

} // namespace ptgn