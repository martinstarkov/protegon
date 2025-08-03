#pragma once

#include "events/event.h"
#include "input/key.h"
#include "input/mouse.h"
#include "math/vector2.h"
#include "serialization/enum.h"

namespace ptgn {

enum class KeyEvent {
	Pressed, /* triggers continuously on key press */
	Down,	 /* triggers once on key press */
	Up,		 /* triggers once on key release */
};

struct KeyDownEvent : public Event {
	explicit KeyDownEvent(Key event_key) : key{ event_key } {}

	Key key;
};

struct KeyUpEvent : public Event {
	explicit KeyUpEvent(Key event_key) : key{ event_key } {}

	Key key;
};

struct KeyPressedEvent : public Event {
	explicit KeyPressedEvent(Key event_key) : key{ event_key } {}

	Key key;
};

enum class MouseEvent {
	Move, /* fires repeatedly on mouse or trackpad movement */
	Down,
	Up,
	Pressed,
	Scroll /* fires repeatedly on mouse or trackpad scroll */
};

struct MouseMoveEvent : public Event {};

struct MouseDownEvent : public Event {
	explicit MouseDownEvent(Mouse event_mouse) : mouse{ event_mouse } {}

	Mouse mouse;
};

struct MouseUpEvent : public Event {
	explicit MouseUpEvent(Mouse event_mouse) : mouse{ event_mouse } {}

	Mouse mouse;
};

struct MousePressedEvent : public Event {
	explicit MousePressedEvent(Mouse event_mouse) : mouse{ event_mouse } {}

	Mouse mouse;
};

struct MouseScrollEvent : public Event {
	explicit MouseScrollEvent(const V2_int& event_scroll) : scroll{ event_scroll } {}

	V2_int scroll;
};

enum class WindowEvent {
	Quit,	  /* fires once when the window is quit */
	Resized,  /* fires one or more times (consult SDL_PollEvent rate and game FPS) after size change
				 occurs or resizing  is finished (window is released) */
	Resizing, /* fires repeatedly while window is being resized */
	Drag,	  /* fires while window is being dragged (moved around) */
	Moved,
	Minimized,
	Maximized
};

struct WindowQuitEvent : public Event {};

struct WindowDragEvent : public Event {};

struct WindowMovedEvent : public Event {};

struct WindowResizedEvent : public Event {
	explicit WindowResizedEvent(const V2_int& resized_window_size) : size{ resized_window_size } {}

	V2_int size;
};

struct WindowResizingEvent : public WindowResizedEvent {
	using WindowResizedEvent::WindowResizedEvent;
};

struct WindowMaximizedEvent : public WindowResizedEvent {
	using WindowResizedEvent::WindowResizedEvent;
};

struct WindowMinimizedEvent : public WindowResizedEvent {
	using WindowResizedEvent::WindowResizedEvent;
};

PTGN_SERIALIZER_REGISTER_ENUM(
	KeyEvent,
	{ { KeyEvent::Pressed, "pressed" }, { KeyEvent::Down, "down" }, { KeyEvent::Up, "up" } }
);

PTGN_SERIALIZER_REGISTER_ENUM(
	MouseEvent, { { MouseEvent::Move, "move" },
				  { MouseEvent::Down, "down" },
				  { MouseEvent::Up, "up" },
				  { MouseEvent::Pressed, "pressed" },
				  { MouseEvent::Scroll, "scroll" } }
);

PTGN_SERIALIZER_REGISTER_ENUM(
	WindowEvent, { { WindowEvent::Quit, "quit" },
				   { WindowEvent::Resized, "resized" },
				   { WindowEvent::Resizing, "resizing" },
				   { WindowEvent::Drag, "drag" },
				   { WindowEvent::Moved, "moved" },
				   { WindowEvent::Minimized, "minimized" },
				   { WindowEvent::Maximized, "maximized" } }
);

} // namespace ptgn