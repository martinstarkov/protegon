#pragma once

#include "serialization/enum.h"

namespace ptgn {

enum class Flip {
	None	   = 0,
	Horizontal = 1,
	Vertical   = 2,
	Both	   = 3
};

PTGN_SERIALIZER_REGISTER_ENUM(
	Flip, { { Flip::None, "none" },
			{ Flip::Horizontal, "horizontal" },
			{ Flip::Vertical, "vertical" },
			{ Flip::Both, "both" } }
);

} // namespace ptgn