#pragma once

#include "event/event.h"
#include "event/key.h"
#include "event/mouse.h"
#include "math/vector2.h"

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

namespace impl {

struct MouseEventBase {
	// @return Current mouse position relative to the currently active scene.
	[[nodiscard]] V2_float GetCurrent() const;
	// @return Previous mouse position relative to the currently active scene.
	[[nodiscard]] V2_float GetPrevious() const;
	// @return Get mouse position difference between current and previous frame relative to the
	// currently active scene.
	[[nodiscard]] V2_float GetDifference() const;
};

} // namespace impl

struct MouseMoveEvent : public Event, public impl::MouseEventBase {
	MouseMoveEvent() = default;
};

struct MouseDownEvent : public Event, public impl::MouseEventBase {
	explicit MouseDownEvent(Mouse mouse) : mouse{ mouse } {}

	Mouse mouse;
};

struct MouseUpEvent : public Event, public impl::MouseEventBase {
	explicit MouseUpEvent(Mouse mouse) : mouse{ mouse } {}

	Mouse mouse;
};

struct MouseScrollEvent : public Event, public impl::MouseEventBase {
	explicit MouseScrollEvent(const V2_int& scroll) : scroll{ scroll } {}

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
	explicit WindowResizedEvent(const V2_int& size) : size{ size } {}

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

} // namespace ptgn