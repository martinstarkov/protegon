#pragma once

#include <ostream>

#include "core/log.h"
#include "serialization/json/enum.h"

namespace ptgn {

enum class Mouse {
	Left   = 0, // SDL_BUTTON_LEFT - 1
	Middle = 1, // SDL_BUTTON_MIDDLE - 1
	Right  = 2	// SDL_BUTTON_RIGHT - 1
};

inline std::ostream& operator<<(std::ostream& os, Mouse mouse) {
	switch (mouse) {
		using enum ptgn::Mouse;
		case Left:	 os << "Left"; break;
		case Right:	 os << "Right"; break;
		case Middle: os << "Middle"; break;
		default:	 PTGN_ERROR("Invalid mouse type");
	}

	return os;
}

PTGN_SERIALIZE_ENUM(
	Mouse, { { Mouse::Left, "left" }, { Mouse::Middle, "middle" }, { Mouse::Right, "right" } }
);

} // namespace ptgn