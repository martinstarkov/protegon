#pragma once

#include "core/event/event.h"
#include "core/math/vector2.h"

namespace ptgn::event {

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
struct WindowFocusLost : public Event<WindowFocusLost> {
	WindowFocusLost() = default;
};

/// @brief Fired once when the window gains focus.
struct WindowFocusGained : public Event<WindowFocusGained> {
	WindowFocusGained() = default;
};

} // namespace ptgn::event