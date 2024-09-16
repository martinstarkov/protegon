#pragma once

#include "event/key.h"
#include "event/mouse.h"
#include "protegon/event.h"
#include "protegon/vector2.h"

namespace ptgn {

enum class KeyEvent {
	Pressed, /* triggers continuously on key press */
	Down,	 /* triggers once on key press */
	Up,		 /* triggers once on key release */
};

struct KeyDownEvent : public Event {
	explicit KeyDownEvent(Key key) : key{ key } {}

	Key key;
};

struct KeyUpEvent : public Event {
	explicit KeyUpEvent(Key key) : key{ key } {}

	Key key;
};

struct KeyPressedEvent : public Event {
	explicit KeyPressedEvent(Key key) : key{ key } {}

	Key key;
};

enum class MouseEvent {
	Move, /* fires repeatedly on mouse or trackpad movement */
	Down,
	Up,
	Scroll /* fires repeatedly on mouse or trackpad scroll */
};

struct MouseMoveEvent : public Event {
	explicit MouseMoveEvent(const V2_int& previous, const V2_int& current) :
		previous{ previous }, current{ current } {}

	V2_int previous;
	V2_int current;
};

class MouseDownEvent : public Event {
public:
	explicit MouseDownEvent(Mouse mouse, const V2_int& current) :
		mouse{ mouse }, current{ current } {}

	Mouse mouse;
	V2_int current;
};

class MouseUpEvent : public Event {
public:
	explicit MouseUpEvent(Mouse mouse, const V2_int& current) :
		mouse{ mouse }, current{ current } {}

	Mouse mouse;
	V2_int current;
};

struct MouseScrollEvent : public Event {
	explicit MouseScrollEvent(const V2_int& scroll) : scroll{ scroll } {}

	V2_int scroll;
};

enum class WindowEvent {
	Quit,	  /* fires once when the window is quit */
	Resized,  /* fires one or more times (consult SDL_PollEvent rate and game FPS) after size change
				 occurs or resizing  is finished (window is released) */
	Resizing, /* fires repeatedly while window is being resized */
	Drag	  /* fires while window is being dragged (moved around) */
};

class WindowQuitEvent : public Event {
public:
	WindowQuitEvent() = default;
};

class WindowDragEvent : public Event {
public:
	WindowDragEvent() = default;
};

class WindowResizedEvent : public Event {
public:
	explicit WindowResizedEvent(const V2_int& size) : size{ size } {}

	V2_int size;
};

class WindowResizingEvent : public WindowResizedEvent {
public:
	using WindowResizedEvent::WindowResizedEvent;
};

} // namespace ptgn