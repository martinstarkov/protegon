#pragma once

#include "mouse.h"
#include "key.h"
#include "time.h"
#include "vector2.h"

// TODO: Consider adding scene camera relative position fetch?

namespace ptgn {

namespace input {

// Retrieves and updates latest user key strokes.
void Update();

V2_int GetMousePosition();

// @return The amount scrolled by the mouse vertically in the current frame, positive upward, negative downward.
int MouseScroll();

MouseState GetMouseState(Mouse button);

bool MousePressed(Mouse button);

bool MouseReleased(Mouse button);

bool MouseDown(Mouse button);

bool MouseUp(Mouse button);

bool KeyPressed(Key key);

bool KeyReleased(Key key);

bool KeyDown(Key key);

bool KeyUp(Key key);

milliseconds GetMouseHeldTime(Mouse button);

/*
* @tparam Duration The unit of time measurement.
* @return True if the mouse button has been held for the given amount of time.
*/
template <typename Duration,
	type_traits::duration<Duration> = true>
inline bool MouseHeld(Mouse button, Duration time) {
	const auto held_time{ GetMouseHeldTime(button) };
	return held_time > time;
}

} // namespace input

} // namespace ptgn