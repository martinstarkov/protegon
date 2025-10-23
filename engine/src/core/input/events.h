#pragma once

#include <variant>
#include <vector>

#include "core/input/key.h"
#include "core/input/mouse.h"
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

struct MouseMove {};

struct MouseDown {
	Mouse button;
};

struct MouseUp {
	Mouse button;
};

struct MousePressed {
	Mouse button;
};

// struct MouseReleased {
//	Mouse button;
// };

struct MouseScroll {
	V2_int scroll;
};

/* fires once when the window is quit */
struct WindowQuit {};

/* fires one or more times (consult SDL_PollEvent rate and game FPS) after size change
				 occurs or resizing  is finished (window is released) */
struct WindowResized {};

struct WindowMoved {};

/* fires repeatedly while window is being resized */
// struct WindowResizing {
//	V2_int size;
// };

/* fires while window is being dragged (moved around) */
// struct WindowDrag {
//	V2_int position;
// };

struct WindowMaximized {};

struct WindowMinimized {};

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