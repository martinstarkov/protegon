#pragma once

#include "core/input/mouse.h"
#include "core/math/vector2.h"

namespace ptgn::event {

/// @brief Fired every frame that the mouse moves.
struct MouseMove {
	/// @brief Relative to the center of the window, with positive x to the right and positive y
	/// down.
	V2_float position;

	V2_float delta;

	operator V2_float() const { // NOSONAR
		return position;
	}
};

/// @brief Fired once during the frame when a mouse button is first pressed down.
struct MousePressed {
	Mouse button;

	/// @brief Relative to the center of the window, with positive x to the right and positive y
	/// down.
	V2_float position;

	operator Mouse() const { // NOSONAR
		return button;
	}
};

/// @brief Fired every frame while a mouse button is held down including the initial press.
struct MouseHeld {
	Mouse button;

	/// @brief Relative to the center of the window, with positive x to the right and positive y
	/// down.
	V2_float position;

	operator Mouse() const { // NOSONAR
		return button;
	}
};

/// @brief Fired once during the frame when a mouse button is released after being pressed down.
struct MouseReleased {
	Mouse button;

	/// @brief Relative to the center of the window, with positive x to the right and positive y
	/// down.
	V2_float position;

	operator Mouse() const { // NOSONAR
		return button;
	}
};

/// @brief Fired every frame that the mouse wheel is scrolled.
struct MouseScroll {
	V2_float scroll;

	/// @brief Relative to the center of the window, with positive x to the right and positive y
	/// down.
	V2_float position;
};

} // namespace ptgn::event