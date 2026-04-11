#pragma once

#include "core/event/event.h"
#include "core/math/vector2.h"
#include "platform/key.h"
#include "platform/mouse.h"

namespace ptgn::event {

/// @brief Fired once during the frame when a key is first pressed down.
struct KeyPressed : public Event<KeyPressed> {
	KeyPressed() = default;

	explicit KeyPressed(Key key) : key{ key } {}

	Key key;

	operator Key() const { // NOSONAR
		return key;
	}
};

/// @brief Fired every frame while a key is held down including the initial press.
struct KeyHeld : public Event<KeyHeld> {
	KeyHeld() = default;

	explicit KeyHeld(Key key) : key{ key } {}

	Key key;

	operator Key() const { // NOSONAR
		return key;
	}
};

/// @brief Fired once during the frame when a key is released after being pressed down.
struct KeyReleased : public Event<KeyReleased> {
	KeyReleased() = default;

	explicit KeyReleased(Key key) : key{ key } {}

	Key key;

	operator Key() const { // NOSONAR
		return key;
	}
};

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

/// @brief Fired once when the window is quit.
struct WindowQuit : public Event<WindowQuit> {};

/// @brief Fired one or more times after size change
/// occurs or resizing is finished (window is released).
struct WindowResized : public Event<WindowResized> {
	WindowResized() = default;

	explicit WindowResized(V2_int size) : size{ size } {}

	V2_int size;
};

/// @brief Fired every time the window is moved.
struct WindowMoved : public Event<WindowMoved> {
	WindowMoved() = default;

	explicit WindowMoved(V2_int position) : position{ position } {}

	V2_int position;
};

/// @brief Fired once when the window is maximized.
struct WindowMaximized : public Event<WindowMaximized> {
	WindowMaximized() = default;

	explicit WindowMaximized(V2_int size) : size{ size } {}

	V2_int size;
};

/// @brief Fired once when the window is minimized.
struct WindowMinimized : public Event<WindowMinimized> {
	WindowMinimized() = default;

	explicit WindowMinimized(V2_int size) : size{ size } {}

	V2_int size;
};

/// @brief Fired once when the window loses focus.
struct WindowFocusLost : public Event<WindowFocusLost> {};

/// @brief Fired once when the window gains focus.
struct WindowFocusGained : public Event<WindowFocusGained> {};

} // namespace ptgn::event