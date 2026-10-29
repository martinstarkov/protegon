#pragma once

#include "runtime/ecs/entity.h"

namespace ptgn::event {

struct PickupFromDropzone {
	operator Entity() const { // NOSONAR
		return draggable;
	}

	/// @brief The draggable that was picked up from the dropzone.
	Entity draggable;
};

struct DropIntoDropzone {
	operator Entity() const { // NOSONAR
		return draggable;
	}

	/// @brief The draggable that was dropped into the dropzone.
	Entity draggable;
};

struct EnterDropzone {
	operator Entity() const { // NOSONAR
		return draggable;
	}

	/// @brief The draggable that entered the dropzone.
	Entity draggable;
};

struct LeaveDropzone {
	operator Entity() const { // NOSONAR
		return draggable;
	}

	/// @brief The draggable that left the dropzone.
	Entity draggable;
};

struct MoveOverDropzone {
	operator Entity() const { // NOSONAR
		return draggable;
	}

	/// @brief The draggable that is over the dropzone.
	Entity draggable;
};

struct MoveOutsideDropzone {
	operator Entity() const { // NOSONAR
		return draggable;
	}

	/// @brief The draggable that is outside the dropzone.
	Entity draggable;
};

} // namespace ptgn::event