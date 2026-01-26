#pragma once

#include <ostream>

#include "core/log.h"
#include "serialization/json/enum.h"

namespace ptgn {

enum class Mouse {
	Invalid = 0,
	Left	= 1, // SDL_BUTTON_LEFT
	Middle	= 2, // SDL_BUTTON_MIDDLE
	Right	= 3	 // SDL_BUTTON_RIGHT
};

inline std::ostream& operator<<(std::ostream& os, Mouse mouse) {
	switch (mouse) {
		using enum ptgn::Mouse;
		case Left:	  os << "Left"; break;
		case Right:	  os << "Right"; break;
		case Middle:  os << "Middle"; break;
		case Invalid: [[fallthrough]];
		default:	  PTGN_ERROR("Invalid mouse type");
	}

	return os;
}

PTGN_SERIALIZE_ENUM(
	Mouse, { { Mouse::Invalid, nullptr },
			 { Mouse::Left, "left" },
			 { Mouse::Middle, "middle" },
			 { Mouse::Right, "right" } }
);

} // namespace ptgn