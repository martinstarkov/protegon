#pragma once

#include "serialization/serialize.h"

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

PTGN_REFLECT_ENUM(Mouse);

} // namespace ptgn