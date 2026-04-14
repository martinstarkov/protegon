#pragma once

#include "core/math/vector2.h"

namespace ptgn::event {

/// @brief Fired one or more times after size change
/// occurs or resizing is finished (window is released).
struct WindowResized {
	V2_int size;

	operator V2_int() const { // NOSONAR
		return size;
	}
};

/// @brief Fired every time the window is moved.
struct WindowMoved {
	V2_int position;

	operator V2_int() const { // NOSONAR
		return position;
	}
};

/// @brief Fired once when the window is maximized.
struct WindowMaximized {
	V2_int size;
};

/// @brief Fired once when the window is minimized.
struct WindowMinimized {
	V2_int size;
};

/// @brief Fired once when the window is quit.
struct WindowQuit {};

/// @brief Fired once when the window loses focus.
struct WindowFocusLost {};

/// @brief Fired once when the window gains focus.
struct WindowFocusGained {};

} // namespace ptgn::event