#pragma once

#include "core/math/vector2.h"
#include "runtime/ecs/entity.h"

namespace ptgn::event {

struct DragStart {
	operator V2_float() const { // NOSONAR
		return start_position;
	}

	/// @brief Position of the mouse in world coordinates at the start of the drag.
	V2_float start_position;
};

struct Drag {
	operator V2_float() const { // NOSONAR
		return position;
	}

	/// @brief Current position of the mouse in world coordinates relative to the camera the entity
	/// is being dragged in.
	V2_float position;

	/// @brief Current offset of the mouse position relative to where it started in world
	/// coordinates.
	V2_float offset;
};

struct DragStop {
	operator V2_float() const { // NOSONAR
		return stop_position;
	}

	/// @brief Position of the mouse in world coordinates at the end of the drag.
	V2_float stop_position;
};

struct PickupDraggable {
	operator Entity() const { // NOSONAR
		return dropzone;
	}

	/// @brief The dropzone that the draggable was picked up from.
	Entity dropzone;
};

struct DropDraggable {
	operator Entity() const { // NOSONAR
		return dropzone;
	}

	/// @brief The dropzone that the draggable was dropped into.
	Entity dropzone;
};

struct DragEnter {
	operator Entity() const { // NOSONAR
		return dropzone;
	}

	/// @brief The dropzone that the draggable entered.
	Entity dropzone;
};

struct DragLeave {
	operator Entity() const { // NOSONAR
		return last_dropzone;
	}

	/// @brief The dropzone that the draggable left.
	Entity last_dropzone;
};

struct DragOver {
	operator Entity() const { // NOSONAR
		return dropzone;
	}

	/// @brief The dropzone that the draggable was dragged over.
	Entity dropzone;
};

struct DragOut {
	operator Entity() const { // NOSONAR
		return dropzone;
	}

	/// @brief The dropzone that the draggable was dragged outside of.
	Entity dropzone;
};

} // namespace ptgn::event