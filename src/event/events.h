#pragma once

#include "event/event.h"
#include "event/key.h"
#include "event/mouse.h"
#include "math/vector2.h"
#include "renderer/render_target.h"

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
	// @return Current mouse position relative to the given render layer.
	[[nodiscard]] V2_float GetCurrent(const RenderTarget& render_target = {}) const;
	// @return Previous mouse position relative to the given render layer.
	[[nodiscard]] V2_float GetPrevious(const RenderTarget& render_target = {}) const;
	// @return Get mouse position difference between current and previous frame relative to the
	// given render layer.
	[[nodiscard]] V2_float GetDifference(const RenderTarget& render_target = {}) const;
};

} // namespace impl

struct MouseMoveEvent : public Event, public impl::MouseEventBase {
	MouseMoveEvent() = default;
};

class MouseDownEvent : public Event, public impl::MouseEventBase {
public:
	explicit MouseDownEvent(Mouse mouse) : mouse{ mouse } {}

	Mouse mouse;
};

class MouseUpEvent : public Event, public impl::MouseEventBase {
public:
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

class WindowQuitEvent : public Event {
public:
	WindowQuitEvent() = default;
};

class WindowDragEvent : public Event {
public:
	WindowDragEvent() = default;
};

class WindowMovedEvent : public Event {
public:
	WindowMovedEvent() = default;
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

class WindowMaximizedEvent : public WindowResizedEvent {
public:
	using WindowResizedEvent::WindowResizedEvent;
};

class WindowMinimizedEvent : public WindowResizedEvent {
public:
	using WindowResizedEvent::WindowResizedEvent;
};

} // namespace ptgn