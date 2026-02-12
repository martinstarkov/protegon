#pragma once

#include "core/event/event.h"
#include "core/math/vector2.h"
#include "platform/input/key.h"
#include "platform/input/mouse.h"

namespace ptgn {

struct KeyDown : public Event<KeyDown> {
	Key key;
	/// True if key is held for more than one frame in a row.
	bool held{ false };

	bool operator==(Key k) const {
		return key == k;
	}

	/// First frame only
	bool IsPressed(Key k) const {
		return !held && key == k;
	}

	/// Repeat frames
	bool IsHeld(Key k) const {
		return held && key == k;
	}
};

struct KeyUp : public Event<KeyUp> {
	Key key;

	bool operator==(Key k) const {
		return key == k;
	}

	bool IsUp(Key k) const {
		return key == k;
	}
};

struct MouseMove : public Event<MouseMove> {
	V2_int position;
	V2_int difference;
};

struct MouseDown : public Event<MouseDown> {
	Mouse button;
	V2_int position;
	/// True if mouse is held for more than one frame in a row.
	bool held{ false };

	bool operator==(Mouse b) const {
		return button == b;
	}

	/// First frame only
	bool IsPressed(Mouse b) const {
		return !held && button == b;
	}

	/// Repeat frames
	bool IsHeld(Mouse b) const {
		return held && button == b;
	}
};

struct MouseUp : public Event<MouseUp> {
	Mouse button;
	V2_int position;

	bool operator==(Mouse b) const {
		return button == b;
	}

	bool IsUp(Mouse b) const {
		return button == b;
	}
};

struct MouseScroll : public Event<MouseScroll> {
	V2_int scroll;
	V2_int position;
};

// Fires once when the window is quit
struct WindowQuit : public Event<WindowQuit> {};

// Fires one or more times (consult SDL_PollEvent rate and game FPS) after size change occurs or
// resizing is finished (window is released).
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