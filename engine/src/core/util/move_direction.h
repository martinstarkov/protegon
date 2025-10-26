#pragma once

#include <ostream>

#include "debug/core/log.h"
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
		case MoveDirection::None:	   os << "None"; break;
		case MoveDirection::UpLeft:	   os << "Up Left"; break;
		case MoveDirection::Up:		   os << "Up"; break;
		case MoveDirection::UpRight:   os << "Up Right"; break;
		case MoveDirection::Left:	   os << "Left"; break;
		case MoveDirection::Right:	   os << "Right"; break;
		case MoveDirection::DownLeft:  os << "Down Left"; break;
		case MoveDirection::Down:	   os << "Down"; break;
		case MoveDirection::DownRight: os << "Down Right"; break;
		default:					   PTGN_ERROR("Invalid movement direction");
	}

	return os;
}

PTGN_SERIALIZER_REGISTER_ENUM(
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