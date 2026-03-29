#pragma once

#include <ostream>
#include <utility>

#include "core/log.h"
#include "serialization/json/enum.h"

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

inline std::ostream& operator<<(std::ostream& os, MoveDirection direction) {
	switch (direction) {
		using enum MoveDirection;
		case None:		return os << "None";
		case UpLeft:	return os << "UpLeft";
		case Up:		return os << "Up";
		case UpRight:	return os << "UpRight";
		case Left:		return os << "Left";
		case Right:		return os << "Right";
		case DownLeft:	return os << "DownLeft";
		case Down:		return os << "Down";
		case DownRight: return os << "DownRight";
		default:		PTGN_ERROR("Unknown MoveDirection: ", std::to_underlying(direction));
	}
}

PTGN_SERIALIZE_ENUM(
	MoveDirection, { { MoveDirection::None, "none" },
					 { MoveDirection::Up, "up" },
					 { MoveDirection::Right, "right" },
					 { MoveDirection::Down, "down" },
					 { MoveDirection::Left, "left" },
					 { MoveDirection::UpLeft, "up_left" },
					 { MoveDirection::UpRight, "up_right" },
					 { MoveDirection::DownRight, "down_right" },
					 { MoveDirection::DownLeft, "down_left" } }
);

} // namespace ptgn