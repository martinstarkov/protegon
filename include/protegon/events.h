#pragma once

#include "protegon/event.h"
#include "protegon/mouse.h"
#include "protegon/vector2.h"

namespace ptgn {

enum class MouseEvent {
	Move,
	Down,
	Up
};

struct MouseMoveEvent : public Event<MouseEvent> {
	MouseMoveEvent(const V2_int& previous, const V2_int& current) :
		previous{ previous }, current{ current }, Event{ MouseEvent::Move } {}

	V2_int previous;
	V2_int current;
};

class MouseDownEvent : public Event<MouseEvent> {
public:
	MouseDownEvent(Mouse mouse, const V2_int& current) :
		mouse{ mouse }, current{ current }, Event{ MouseEvent::Down } {}

	Mouse mouse;
	V2_int current;
};

class MouseUpEvent : public Event<MouseEvent> {
public:
	MouseUpEvent(Mouse mouse, const V2_int& current) :
		mouse{ mouse }, current{ current }, Event{ MouseEvent::Up } {}

	Mouse mouse;
	V2_int current;
};

enum class WindowEvent {
	Quit
};

class WindowQuitEvent : public Event<WindowEvent> {
public:
	WindowQuitEvent() : Event{ WindowEvent::Quit } {}
};

} // namespace ptgn