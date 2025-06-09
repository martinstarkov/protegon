#pragma once

#include <ostream>

#include "debug/log.h"

namespace ptgn {

enum class MoveDirection {
	Up,
	Right,
	Down,
	Left,
	UpLeft,
	UpRight,
	DownRight,
	DownLeft,
	None
};

inline std::ostream& operator<<(std::ostream& os, MoveDirection direction) {
	switch (direction) {
		case MoveDirection::UpLeft:	   os << "Up Left"; break;
		case MoveDirection::Up:		   os << "Up"; break;
		case MoveDirection::UpRight:   os << "Up Right"; break;
		case MoveDirection::Left:	   os << "Left"; break;
		case MoveDirection::None:	   os << "None"; break;
		case MoveDirection::Right:	   os << "Right"; break;
		case MoveDirection::DownLeft:  os << "Down Left"; break;
		case MoveDirection::Down:	   os << "Down"; break;
		case MoveDirection::DownRight: os << "Down Right"; break;
		default:					   PTGN_ERROR("Invalid movement direction")
	}

	return os;
}

} // namespace ptgn