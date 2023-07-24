#pragma once

#include "protegon/event.h"
#include "protegon/mouse.h"
#include "protegon/vector2.h"

namespace ptgn {

enum class MouseEvent {
	MOVE,
	DOWN,
	UP
};

struct MouseMoveEvent : public Event<MouseEvent> {
	MouseMoveEvent(const V2_int& previous, const V2_int& current) :
		previous{ previous },
		current{ current },
		Event{ MouseEvent::MOVE } {}
	V2_int previous;
	V2_int current;
};

class MouseDownEvent : public Event<MouseEvent> {
public:
	MouseDownEvent(Mouse mouse, const V2_int& current) :
		mouse{ mouse },
		current{ current },
		Event{ MouseEvent::DOWN } {}
	Mouse mouse;
	V2_int current;
};

class MouseUpEvent : public Event<MouseEvent> {
public:
	MouseUpEvent(Mouse mouse, const V2_int& current) :
		mouse{ mouse },
		current{ current },
		Event{ MouseEvent::UP } {}
	Mouse mouse;
	V2_int current;
};

} // namespace ptgn