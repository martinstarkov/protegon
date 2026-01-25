#pragma once

#include "core/event/event.h"
#include "core/input/key.h"
#include "core/input/mouse.h"
#include "math/vector2.h"

namespace ptgn {

struct KeyDown : public Event<KeyDown> {
	Key key;
	// @return True if key is held for more than one frame in a row.
	bool held{ false };
};

struct KeyUp : public Event<KeyUp> {
	Key key;
};

struct MouseMove : public Event<MouseMove> {
	V2_int position;
	V2_int difference;
};

struct MouseDown : public Event<MouseDown> {
	Mouse button;
	V2_int position;
	// @return True if mouse is held for more than one frame in a row.
	bool held{ false };
};

struct MouseUp : public Event<MouseUp> {
	Mouse button;
	V2_int position;
};

struct MouseScroll : public Event<MouseScroll> {
	V2_int scroll;
	V2_int position;
};

// Fires once when the window is quit
struct WindowQuit : public Event<WindowQuit> {};

// Fires one or more times (consult SDL_PollEvent rate and game FPS) after size change occurs or
// resizing  is finished (window is released)
struct WindowResized : public Event<WindowResized> {
	V2_int size;
};

struct WindowMoved : public Event<WindowMoved> {
	V2_int position;
};

struct WindowMaximized : public Event<WindowMaximized> {
	V2_int size;
};

struct WindowMinimized : public Event<WindowMinimized> {
	V2_int size;
};

struct WindowFocusLost : public Event<WindowFocusLost> {};

struct WindowFocusGained : public Event<WindowFocusGained> {};

} // namespace ptgn