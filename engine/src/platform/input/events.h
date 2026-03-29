#pragma once

#include "core/event/event.h"
#include "core/math/vector2.h"
#include "platform/input/key.h"
#include "platform/input/mouse.h"

namespace ptgn {

/// @brief Fired once during the frame when a key is first pressed down.
struct KeyPressed : public Event<KeyPressed> {
	Key key;

	bool operator==(Key k) const {
		return key == k;
	}
};

/// @brief Fired every frame while a key is held down including the initial press.
struct KeyHeld : public Event<KeyHeld> {
	Key key;

	bool operator==(Key k) const {
		return key == k;
	}
};

/// @brief Fired once during the frame when a key is released after being pressed down.
struct KeyReleased : public Event<KeyReleased> {
	Key key;

	bool operator==(Key k) const {
		return key == k;
	}
};

/// @brief Fired every frame that the mouse moves.
struct MouseMove : public Event<MouseMove> {
	/// @brief Relative to the center of the window, with positive x to the right and positive y
	/// down.
	V2_int position;
	V2_int delta;
};

/// @brief Fired once during the frame when a mouse button is first pressed down.
struct MousePressed : public Event<MousePressed> {
	Mouse button;

	/// @brief Relative to the center of the window, with positive x to the right and positive y
	/// down.
	V2_int position;

	bool operator==(Mouse b) const {
		return button == b;
	}
};

/// @brief Fired every frame while a mouse button is held down including the initial press.
struct MouseHeld : public Event<MouseHeld> {
	Mouse button;

	/// @brief Relative to the center of the window, with positive x to the right and positive y
	/// down.
	V2_int position;

	bool operator==(Mouse b) const {
		return button == b;
	}
};

/// @brief Fired once during the frame when a mouse button is released after being pressed down.
struct MouseReleased : public Event<MouseReleased> {
	Mouse button;

	/// @brief Relative to the center of the window, with positive x to the right and positive y
	/// down.
	V2_int position;

	bool operator==(Mouse b) const {
		return button == b;
	}
};

/// @brief Fired every frame that the mouse wheel is scrolled.
struct MouseScroll : public Event<MouseScroll> {
	V2_int scroll;

	/// @brief Relative to the center of the window, with positive x to the right and positive y
	/// down.
	V2_int position;
};

/// @brief Fired once when the window is quit.
struct WindowQuit : public Event<WindowQuit> {};

/// @brief Fired one or more times (consult SDL_PollEvent rate and game FPS) after size change
/// occurs or resizing is finished (window is released).
struct WindowResized : public Event<WindowResized> {
	V2_int size;
};

/// @brief Fired every time the window is moved.
struct WindowMoved : public Event<WindowMoved> {
	V2_int position;
};

/// @brief Fired once when the window is maximized.
struct WindowMaximized : public Event<WindowMaximized> {
	V2_int size;
};

/// @brief Fired once when the window is minimized.
struct WindowMinimized : public Event<WindowMinimized> {
	V2_int size;
};

/// @brief Fired once when the window loses focus.
struct WindowFocusLost : public Event<WindowFocusLost> {};

/// @brief Fired once when the window gains focus.
struct WindowFocusGained : public Event<WindowFocusGained> {};

} // namespace ptgn