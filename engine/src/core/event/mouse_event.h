#pragma once

#include "core/event/event.h"
#include "core/input/mouse.h"
#include "core/math/vector2.h"

namespace ptgn::event {

/// @brief Fired every frame that the mouse moves.
struct MouseMove : public Event<MouseMove> {
	MouseMove() = default;

	MouseMove(V2_float position, V2_float delta) : position{ position }, delta{ delta } {}

	/// @brief Relative to the center of the window, with positive x to the right and positive y
	/// down.
	V2_float position;

	V2_float delta;
};

/// @brief Fired once during the frame when a mouse button is first pressed down.
struct MousePressed : public Event<MousePressed> {
	MousePressed() = default;

	MousePressed(Mouse button, V2_float position) : button{ button }, position{ position } {}

	Mouse button;

	/// @brief Relative to the center of the window, with positive x to the right and positive y
	/// down.
	V2_float position;

	operator Mouse() const { // NOSONAR
		return button;
	}
};

/// @brief Fired every frame while a mouse button is held down including the initial press.
struct MouseHeld : public Event<MouseHeld> {
	MouseHeld() = default;

	MouseHeld(Mouse button, V2_float position) : button{ button }, position{ position } {}

	Mouse button;

	/// @brief Relative to the center of the window, with positive x to the right and positive y
	/// down.
	V2_float position;

	operator Mouse() const { // NOSONAR
		return button;
	}
};

/// @brief Fired once during the frame when a mouse button is released after being pressed down.
struct MouseReleased : public Event<MouseReleased> {
	MouseReleased() = default;

	MouseReleased(Mouse button, V2_float position) : button{ button }, position{ position } {}

	Mouse button;

	/// @brief Relative to the center of the window, with positive x to the right and positive y
	/// down.
	V2_float position;

	operator Mouse() const { // NOSONAR
		return button;
	}
};

/// @brief Fired every frame that the mouse wheel is scrolled.
struct MouseScroll : public Event<MouseScroll> {
	MouseScroll() = default;

	MouseScroll(V2_float scroll, V2_float position) : scroll{ scroll }, position{ position } {}

	V2_float scroll;

	/// @brief Relative to the center of the window, with positive x to the right and positive y
	/// down.
	V2_float position;
};

} // namespace ptgn::event