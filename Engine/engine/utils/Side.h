#pragma once

#include "Direction.h"

namespace engine {

struct Side {
	Side() = default;
	Side(Direction side) : side{ side } {}
	// Return the opposite side.
	Direction GetOpposite() {
		switch (side) {
			case Direction::RIGHT:
				return Direction::LEFT;
			case Direction::LEFT:
				return Direction::RIGHT;
			case Direction::UP:
				return Direction::DOWN;
			case Direction::DOWN:
				return Direction::UP;
			case Direction::NONE:
			default:
				break;
		}
		return Direction::NONE;
	}
	// Return the two sides perpendicular to this one.
	std::pair<Direction, Direction> GetPerpendicular() {
		switch (side) {
			case Direction::LEFT:
			case Direction::RIGHT:
				return { Direction::UP, Direction::DOWN };
			case Direction::DOWN:
			case Direction::UP:
				return { Direction::LEFT, Direction::RIGHT };
			case Direction::NONE:
			default:
				break;
		}
		return { Direction::NONE, Direction::NONE };
	}
	bool IsTop() { return side == Direction::TOP; }
	bool IsBottom() { return side == Direction::BOTTOM; }
	bool IsLeft() { return side == Direction::LEFT; }
	bool IsRight() { return side == Direction::RIGHT; }
	bool IsVertical() { return IsTop() || IsBottom(); }
	bool IsHorizontal() { return IsLeft() || IsRight(); }
	operator bool() const { return side != Direction::NONE; }
	Direction side = Direction::NONE;
};

inline bool operator==(const Side& lhs, const Side& rhs) {
	return lhs.side == rhs.side;
}

inline bool operator==(const Side& lhs, const Direction& rhs) {
	return lhs.side == rhs;
}

inline bool operator==(const Direction& lhs, const Side& rhs) {
	return rhs == lhs;
}

} // namespace engine