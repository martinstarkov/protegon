#pragma once

#include "protegon/event.h"
#include "event/mouse.h"
#include "protegon/vector2.h"

namespace ptgn {

enum class MouseEvent {
	Move,
	Down,
	Up,
	Scroll
};

struct MouseMoveEvent : public Event {
	MouseMoveEvent(const V2_int& previous, const V2_int& current) :
		previous{ previous }, current{ current } {}

	V2_int previous;
	V2_int current;
};

class MouseDownEvent : public Event {
public:
	MouseDownEvent(Mouse mouse, const V2_int& current) :
		mouse{ mouse }, current{ current } {}

	Mouse mouse;
	V2_int current;
};

class MouseUpEvent : public Event {
public:
	MouseUpEvent(Mouse mouse, const V2_int& current) :
		mouse{ mouse }, current{ current } {}

	Mouse mouse;
	V2_int current;
};

struct MouseScrollEvent : public Event {
	MouseScrollEvent(const V2_int& scroll) : scroll{ scroll } {}

	V2_int scroll;
};

enum class WindowEvent {
	Quit,
	Resize
};

class WindowQuitEvent : public Event {
public:
	WindowQuitEvent() = default;
};

class WindowResizeEvent : public Event {
public:
	WindowResizeEvent(const V2_int& size) : size{ size } {}
	V2_int size;
};

} // namespace ptgn