#pragma once

#include "core/math/vector2.h"
#include "runtime/physics/move_direction.h"

namespace ptgn::event {

struct PlayerMoveStart {
	MoveDirection direction;		 // Direction at start

	operator MoveDirection() const { // NOSONAR
		return direction;
	}
};

struct PlayerMoveHeld {
	MoveDirection direction;		 // Current direction

	operator MoveDirection() const { // NOSONAR
		return direction;
	}
};

struct PlayerMoveStop {
	MoveDirection last_direction;	 // Direction before stopping

	operator MoveDirection() const { // NOSONAR
		return last_direction;
	}
};

struct PlayerMoveDirectionChange {
	V2_float difference;
	MoveDirection current_direction; // Resulting direction

	operator MoveDirection() const { // NOSONAR
		return current_direction;
	}
};

struct PlayerMoveDirectionStart {
	MoveDirection direction;

	operator MoveDirection() const { // NOSONAR
		return direction;
	}
};

struct PlayerMoveDirectionHeld {
	MoveDirection direction;

	operator MoveDirection() const { // NOSONAR
		return direction;
	}
};

struct PlayerMoveDirectionStop {
	MoveDirection direction;

	operator MoveDirection() const { // NOSONAR
		return direction;
	}
};

} // namespace ptgn::event