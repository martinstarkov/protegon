#pragma once

#include "serialization/serialize.h"

namespace ptgn {

enum class MoveDirection {
	None,
	Up,
	Right,
	Down,
	Left,
	UpLeft,
	UpRight,
	DownRight,
	DownLeft
};
PTGN_SERIALIZE_ENUM(MoveDirection);

} // namespace ptgn