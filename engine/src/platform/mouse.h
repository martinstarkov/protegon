#pragma once

#include <ostream>
#include <utility>

#include "core/log.h"
#include "serialization/json/enum.h"

namespace ptgn {

enum class Mouse {
	Button0 = 0,
	Button1 = 1,
	Button2 = 2,
	Button3 = 3,
	Button4 = 4,
	Button5 = 5,
	Button6 = 6,
	Button7 = 7,

	Last   = Button7,
	Left   = Button0,
	Right  = Button1,
	Middle = Button2
};

inline std::ostream& operator<<(std::ostream& os, Mouse mouse) {
	switch (mouse) {
		using enum Mouse;
		case Left:	  return os << "Left";
		case Right:	  return os << "Right";
		case Middle:  return os << "Middle";
		case Last:	  return os << "Last";
		case Button3: return os << "Button3";
		case Button4: return os << "Button4";
		case Button5: return os << "Button5";
		case Button6: return os << "Button6";
		default:	  PTGN_ERROR("Unknown Mouse: ", std::to_underlying(mouse));
	}
}

PTGN_SERIALIZE_ENUM(
	Mouse, { { Mouse::Left, "left" },
			 { Mouse::Middle, "middle" },
			 { Mouse::Right, "right" },
			 { Mouse::Last, "last" },
			 { Mouse::Button3, "button3" },
			 { Mouse::Button4, "button4" },
			 { Mouse::Button5, "button5" },
			 { Mouse::Button6, "button6" } }
);

} // namespace ptgn