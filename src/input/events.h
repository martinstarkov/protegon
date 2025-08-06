#pragma once

#include <variant>
#include <vector>

#include "input/key.h"
#include "input/mouse.h"
#include "math/vector2.h"

namespace ptgn::impl {

struct KeyDown {
	Key key;
};

struct KeyPressed {
	Key key;
};

struct KeyUp {
	Key key;
};

// struct KeyReleased {
//	Key key;
// };

struct MouseMove {
	V2_int position;
	V2_int difference;
};

struct MouseDown {
	Mouse button;
	V2_int position;
};

struct MouseUp {
	Mouse button;
	V2_int position;
};

struct MousePressed {
	Mouse button;
	V2_int position;
};

// struct MouseReleased {
//	Mouse button;
//	V2_int position;
// };

struct MouseScroll {
	V2_int scroll;
	V2_int position;
};

/* fires once when the window is quit */
struct WindowQuit {};

/* fires one or more times (consult SDL_PollEvent rate and game FPS) after size change
				 occurs or resizing  is finished (window is released) */
struct WindowResized {
	V2_int size;
};

struct WindowMoved {
	V2_int position;
};

/* fires repeatedly while window is being resized */
// struct WindowResizing {
//	V2_int size;
// };

/* fires while window is being dragged (moved around) */
// struct WindowDrag {
//	V2_int position;
// };

struct WindowMaximized {
	V2_int size;
};

struct WindowMinimized {
	V2_int size;
};

struct WindowFocusLost {};

struct WindowFocusGained {};

using InputEvent = std::variant<
	KeyDown, KeyPressed, KeyUp /*, KeyReleased*/, MouseMove, MouseDown, MousePressed, MouseUp /*,
	  MouseReleased*/
	,
	MouseScroll, WindowResized, WindowMoved, WindowMaximized, WindowMinimized, WindowFocusLost,
	WindowFocusGained, WindowQuit
	/*, WindowResizing, WindowDrag*/>;

using InputQueue = std::vector<InputEvent>;

} // namespace ptgn::impl