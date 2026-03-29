#pragma once

#include <ostream>
#include <utility>

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
		using enum Mouse;
		case Left:	 return os << "Left";
		case Right:	 return os << "Right";
		case Middle: return os << "Middle";
		default:	 PTGN_ERROR("Unknown Mouse: ", std::to_underlying(mouse));
	}
}

PTGN_SERIALIZE_ENUM(
	Mouse, { { Mouse::Left, "left" }, { Mouse::Middle, "middle" }, { Mouse::Right, "right" } }
);

} // namespace ptgn